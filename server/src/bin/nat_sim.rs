//! Standalone NAT simulator binary.
//!
//! Runs a NAT simulator and optional STUN server for manual testing.
//!
//! Usage: nat-sim [--nat-type full-cone|port-restricted|symmetric|symmetric-seq]

use dxx_matchmaking::nat_sim::{start_nat, start_stun_server, NatType};
use std::env;

#[tokio::main]
async fn main() {
    let args: Vec<String> = env::args().collect();
    let nat_type = parse_nat_type(&args);

    println!("Starting NAT simulator ({nat_type:?}) and STUN server...");

    let stun = start_stun_server().await;
    let nat = start_nat(nat_type, 0).await;

    println!("STUN server:    {}", stun.addr);
    println!("NAT internal:   {}", nat.internal_addr);
    println!("NAT external IP: {}", nat.external_ip);
    println!("Press Ctrl-C to stop.");

    tokio::signal::ctrl_c().await.ok();

    nat.abort();
    stun.abort();
}

fn parse_nat_type(args: &[String]) -> NatType {
    let mut i = 1;
    while i < args.len() {
        if args[i] == "--nat-type" && i + 1 < args.len() {
            return match args[i + 1].as_str() {
                "full-cone" => NatType::FullCone,
                "port-restricted" => NatType::PortRestricted,
                "symmetric" => NatType::Symmetric,
                "symmetric-seq" => NatType::SymmetricSequential,
                other => {
                    eprintln!("Unknown NAT type: {other}");
                    eprintln!("Valid: full-cone, port-restricted, symmetric, symmetric-seq");
                    std::process::exit(1);
                }
            };
        }
        i += 1;
    }
    NatType::FullCone
}
