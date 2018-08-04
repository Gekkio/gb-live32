#[macro_use]
extern crate clap;
#[macro_use]
extern crate failure;
extern crate gb_live32;
extern crate itertools;
#[macro_use]
extern crate log;
extern crate rand;
extern crate serialport;
extern crate simplelog;

use clap::{Arg, ArgMatches};
use failure::Error;
use gb_live32::Gbl32;
use rand::rngs::SmallRng;
use rand::{FromEntropy, RngCore};
use serialport::SerialPortType;
use simplelog::{LevelFilter, TermLogger};
use std::ffi::{OsStr, OsString};
use std::fs::File;
use std::io::{self, Read, Write};
use std::process;
use std::thread;

fn scan_ports() -> Result<Vec<OsString>, Error> {
  let ports = serialport::available_ports()?
    .into_iter()
    .filter_map(|port| {
      if let SerialPortType::UsbPort(info) = port.port_type {
        let name = port.port_name;
        let manufacturer = info.manufacturer.as_ref().map_or("", String::as_ref);
        let product = info.product.as_ref().map_or("", String::as_ref);
        let serial_number = info.serial_number.as_ref().map_or("", String::as_ref);
        if info.vid == 0x16c0
          && info.pid == 0x05e1
          && manufacturer == "gekkio.fi"
          && (product == "GB-LIVE32" || product == "GB_LIVE32")
        {
          info!("Detected device: {} ({:04x}:{:04x}, manufacturer=\"{}\", product=\"{}\", serial=\"{}\")",
            name, info.vid, info.pid, manufacturer, product, serial_number);
          Some(name.into())
        } else {
          info!("Skipping device: {} ({:04x}:{:04x}, manufacturer=\"{}\", product=\"{}\", serial=\"{}\")",
            name, info.vid, info.pid, manufacturer, product, serial_number);
          None
        }
      } else {
        None
      }
    })
    .collect::<Vec<_>>();
  Ok(ports)
}

#[derive(Clone, Debug)]
enum Operation {
  Upload(Vec<u8>),
  Status,
}

fn worker(port: &OsString, operation: Operation) -> Result<(), Error> {
  let name = port.to_string_lossy();
  let port = serialport::open(&port)?;
  info!("{}: Connecting...", name);
  let mut gbl32 = Gbl32::from_port(port)?;

  let version = gbl32.get_version()?;
  match version {
    (2, 0) | (2, 1) => (),
    (major, minor) => bail!("{}: Unsupported version v{}.{}", name, major, minor),
  }
  info!("{}: Connected (v{}.{})", name, version.0, version.1);
  unlock_if_necessary(&name, &mut gbl32)?;

  match operation {
    Operation::Upload(data) => {
      assert!(data.len() == 32768);
      gbl32.set_reset(true)?;
      gbl32.set_passthrough(false)?;

      gbl32.write_all(&data)?;

      gbl32.set_passthrough(true)?;
      gbl32.set_reset(false)?;
      info!("{}: Wrote ROM and reset the system", name);
    }
    Operation::Status => {
      let status = gbl32.get_status()?;
      info!(
        "Status: unlocked={}, passthrough={}, reset={}",
        status.unlocked, status.passthrough, status.reset
      );
    }
  }
  Ok(())
}

fn unlock_if_necessary(name: &str, gbl32: &mut Gbl32) -> Result<(), Error> {
  if gbl32.get_status()?.unlocked {
    return Ok(());
  }
  info!("{}: Unlocking...", name);

  gbl32.set_passthrough(false)?;
  gbl32.set_unlocked(true)?;

  let mut buffer = vec![0u8; 0x8000];
  let mut rng = SmallRng::from_entropy();
  rng.fill_bytes(&mut buffer);

  gbl32.write_all(&buffer)?;

  let data = gbl32.read_all()?;

  for (idx, (a, b)) in buffer.into_iter().zip(data.into_iter()).enumerate() {
    if a != b {
      bail!("Self-test failed at index {}", idx);
    }
  }

  if !gbl32.get_status()?.unlocked {
    bail!("Failed to unlock device");
  }
  info!("{}: Unlocked device after self-test", name);
  Ok(())
}

fn run(matches: &ArgMatches) -> Result<(), Error> {
  let _ = TermLogger::init(LevelFilter::Debug, simplelog::Config::default());

  let ports;
  if matches.is_present("broadcast") {
    ports = scan_ports()?;
  } else if let Some(devices) = matches.values_of_os("port") {
    ports = devices.map(OsStr::to_os_string).collect();
  } else {
    ports = scan_ports()?;
    if ports.len() > 1 {
      bail!("Too many detected devices for automatic selection");
    }
  }

  if ports.is_empty() {
    bail!("No supported devices found");
  }

  info!(
    "Using {}: {}",
    if ports.len() == 1 {
      "device"
    } else {
      "devices"
    },
    itertools::join(ports.iter().map(|p| p.to_string_lossy()), ", ")
  );

  let operation = if let Some(path) = matches.value_of_os("upload") {
    let mut file = File::open(path)?;
    let mut buf = vec![0; 0x8000];
    match file.read_exact(&mut buf) {
      Err(ref e) if e.kind() == io::ErrorKind::UnexpectedEof => {
        bail!("ROM file is smaller than 32768 bytes");
      }
      result => result?,
    }
    let mut byte = [0; 1];
    match file.read_exact(&mut byte) {
      Err(ref e) if e.kind() == io::ErrorKind::UnexpectedEof => (),
      Ok(_) => bail!("ROM file is larger than 32768 bytes"),
      result => result?,
    }
    Operation::Upload(buf)
  } else {
    Operation::Status
  };

  let threads = ports
    .into_iter()
    .map(|port| {
      let name = port.to_string_lossy().to_string();
      let op = operation.clone();
      (name, thread::spawn(move || worker(&port, op)))
    })
    .collect::<Vec<_>>();

  let mut failures = 0;
  for (name, thread) in threads {
    let result = thread
      .join()
      .map_err(|_| format_err!("{}: failed to wait for worker thread", name))?;
    if let Err(ref e) = result {
      let stderr = &mut io::stderr();
      let _ = writeln!(stderr, "{}: {}\n{}", name, e, e.backtrace());
      failures += 1;
    }
  }

  if failures > 0 {
    bail!("{} devices failed", failures);
  }
  Ok(())
}

fn main() {
  let matches = app_from_crate!()
    .arg(
      Arg::with_name("broadcast")
        .short("b")
        .long("broadcast")
        .help("Broadcast mode: use all connected devices"),
    )
    .arg(
      Arg::with_name("port")
        .short("p")
        .long("port")
        .value_name("port")
        .multiple(true)
        .help("Serial port to use"),
    )
    .arg(
      Arg::with_name("upload")
        .short("u")
        .long("upload")
        .value_name("upload")
        .help("ROM file to upload"),
    )
    .get_matches();

  if let Err(ref e) = run(&matches) {
    let stderr = &mut io::stderr();
    let _ = writeln!(stderr, "Error: {}\n{}", e, e.backtrace());
    process::exit(1);
  }
}
