extern crate clap;
use clap::Arg;
extern crate time;
#[macro_use]
extern crate slog;

extern crate ccp_pcc;
extern crate portus;

use ccp_pcc::PccConfig;

fn make_args(log: slog::Logger) -> Result<(PccConfig, String), String> {
    let matches = clap::App::new("CCP PCC")
        .version("0.1.1")
        .author("Tong Meng <tongm2@illinois.edu>")
        .about("Implementation of PCC Congestion Control")
        .arg(Arg::with_name("ipc")
             .long("ipc")
             .help("Sets the type of ipc to use: (netlink|unix)")
             .default_value("unix")
             .validator(portus::algs::ipc_valid))
        .get_matches();

    Ok((
        PccConfig {
            logger: Some(log),
        },
        String::from(matches.value_of("ipc").unwrap()),
    ))
}

fn main() {
    let log = portus::algs::make_logger();
    let (cfg, ipc) = make_args(log.clone())
        .map_err(|e| warn!(log, "bad argument"; "err" => ?e))
        .unwrap();

    info!(log, "configured BBR"; 
        "ipc" => ipc.clone(),
    );

    portus::start!(ipc.as_str(), Some(log), cfg).unwrap()
}