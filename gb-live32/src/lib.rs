extern crate bufstream;
extern crate cobs;
#[macro_use]
extern crate failure;
extern crate rand;
extern crate serialport;

use bufstream::BufStream;
use rand::rngs::SmallRng;
use rand::{RngCore, SeedableRng};
use serialport::SerialPort;
use std::io::{self, BufRead, Read, Write};
use std::time::Duration;

#[derive(Fail, Debug)]
pub enum Gbl32Error {
    #[fail(display = "Handshake failed")]
    Handshake,
    #[fail(display = "COBS decode error")]
    Decode,
    #[fail(display = "Protocol error: {}", _0)]
    Protocol(String),
    #[fail(display = "Serial error: {}", _0)]
    Serial(#[cause] serialport::Error),
    #[fail(display = "IO error: {}", _0)]
    Io(#[cause] io::Error),
}

pub struct Gbl32 {
    port: BufStream<Box<dyn SerialPort>>,
    read_buffer: Vec<u8>,
    write_buffer: Box<[u8]>,
}

#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub struct Status {
    pub unlocked: bool,
    pub passthrough: bool,
    pub reset: bool,
}

impl Gbl32 {
    pub fn from_port(mut port: Box<dyn SerialPort>) -> Result<Gbl32, Gbl32Error> {
        port.set_timeout(Duration::from_millis(200))
            .map_err(Gbl32Error::Serial)?;
        let mut gbl32 = Gbl32 {
            port: BufStream::new(port),
            read_buffer: Vec::new(),
            write_buffer: vec![0; 1024].into_boxed_slice(),
        };
        let mut rng = SmallRng::from_entropy();
        let mut handshake = vec![0; 8];
        let mut errors = 0;
        loop {
            rng.fill_bytes(&mut handshake);
            match gbl32.ping(&handshake) {
                Ok(success) => {
                    if success {
                        break;
                    }
                    errors += 1;
                }
                Err(Gbl32Error::Decode) | Err(Gbl32Error::Protocol(_)) => errors += 1,
                Err(Gbl32Error::Io(ref e)) if e.kind() == io::ErrorKind::TimedOut => errors += 1,
                Err(e) => return Err(e),
            }
            if errors > 10 {
                return Err(Gbl32Error::Handshake);
            }
        }
        Ok(gbl32)
    }
    fn request_response(
        &mut self,
        cmd: u8,
        msg: &[u8],
        expected_len: usize,
    ) -> Result<&[u8], Gbl32Error> {
        let mut payload = Vec::from(msg);
        payload.push(cmd);

        let encoded_len = cobs::encode(&payload, &mut self.write_buffer);
        self.write_buffer[encoded_len] = 0x00;
        self.port
            .write_all(&self.write_buffer[0..(encoded_len + 1)])
            .map_err(Gbl32Error::Io)?;
        self.port.flush().map_err(Gbl32Error::Io)?;

        self.read_buffer.clear();
        self.port
            .read_until(0x00, &mut self.read_buffer)
            .map_err(Gbl32Error::Io)?;
        self.read_buffer.pop();

        let decoded_len =
            cobs::decode_in_place(&mut self.read_buffer).map_err(|_| Gbl32Error::Decode)?;
        self.read_buffer.truncate(decoded_len);

        let response_cmd = self.read_buffer.pop();
        let response_result = self.read_buffer.pop();

        match response_cmd {
            Some(response_cmd) if cmd == response_cmd => (),
            Some(response_cmd) => {
                return Err(Gbl32Error::Protocol(format!(
                    "Command byte mismatch {} vs {}",
                    response_cmd, cmd
                )))
            }
            None => return Err(Gbl32Error::Protocol("Missing command byte".to_string())),
        }
        match response_result {
            Some(0xFF) => {
                let len = self.read_buffer.len();
                if len == expected_len {
                    Ok(&self.read_buffer)
                } else {
                    Err(Gbl32Error::Protocol(format!(
                        "Expected {} bytes in response, got {}",
                        expected_len, len
                    )))
                }
            }
            Some(0xFE) => Err(Gbl32Error::Protocol(
                String::from_utf8_lossy(&self.read_buffer).to_string(),
            )),
            Some(result) => Err(Gbl32Error::Protocol(format!("Result {:02x}", result))),
            None => Err(Gbl32Error::Protocol("Missing result byte".to_string())),
        }
    }
    pub fn ping(&mut self, data: &[u8]) -> Result<bool, Gbl32Error> {
        if data.len() != 8 {
            return Err(Gbl32Error::Protocol(format!(
                "Expected 8 bytes for ping, got {}",
                data.len()
            )));
        }
        let response = self.request_response(0x01, data, 8)?;
        Ok(response == data)
    }
    pub fn get_version(&mut self) -> Result<(u8, u8), Gbl32Error> {
        let data = self.request_response(0x02, &[], 2)?;
        Ok((data[0], data[1]))
    }
    pub fn get_status(&mut self) -> Result<Status, Gbl32Error> {
        let data = self.request_response(0x03, &[], 3)?;
        Ok(Status {
            unlocked: data[0] != 0x00,
            passthrough: data[1] != 0x00,
            reset: data[2] != 0x00,
        })
    }
    pub fn set_unlocked(&mut self, value: bool) -> Result<(), Gbl32Error> {
        self.request_response(0x04, &[value as u8], 0)?;
        Ok(())
    }
    pub fn set_passthrough(&mut self, value: bool) -> Result<(), Gbl32Error> {
        self.request_response(0x05, &[value as u8], 0)?;
        Ok(())
    }
    pub fn set_reset(&mut self, value: bool) -> Result<(), Gbl32Error> {
        self.request_response(0x06, &[value as u8], 0)?;
        Ok(())
    }
    pub fn read_block(&mut self, addr_h: u8) -> Result<&[u8], Gbl32Error> {
        let data = self.request_response(0x07, &[addr_h], 256)?;
        Ok(data)
    }
    pub fn write_block(&mut self, addr_h: u8, data: &[u8]) -> Result<(), Gbl32Error> {
        if data.len() != 256 {
            return Err(Gbl32Error::Protocol(format!(
                "Expected 256 bytes for writing, got {}",
                data.len()
            )));
        }
        let mut payload = Vec::with_capacity(257);
        payload.push(addr_h);
        payload.extend(data);
        self.request_response(0x08, &payload, 0)?;
        Ok(())
    }
    pub fn write_all(&mut self, data: &[u8]) -> Result<(), Gbl32Error> {
        if data.len() != 0x8000 {
            return Err(Gbl32Error::Protocol(format!(
                "Expected 32768 bytes for writing, got {}",
                data.len()
            )));
        }
        self.request_response(0x09, &[], 0)?;
        self.port.write_all(data).map_err(Gbl32Error::Io)?;
        self.port.flush().map_err(Gbl32Error::Io)?;
        Ok(())
    }
    pub fn read_all(&mut self) -> Result<Vec<u8>, Gbl32Error> {
        self.request_response(0x0a, &[], 0)?;
        let mut buf = vec![0; 0x8000];
        self.port.read_exact(&mut buf).map_err(Gbl32Error::Io)?;
        Ok(buf)
    }
}
