#[macro_use]
extern crate slog;
extern crate fnv;
extern crate portus;
extern crate time;

use portus::ipc::Ipc;
use portus::lang::Scope;
use portus::{CongAlg, Datapath, DatapathInfo, DatapathTrait, Report};
use std::collections::HashMap;

pub struct Pcc<T: Ipc> {
    control_channel: Datapath<T>,
    logger: Option<slog::Logger>,
    sc: Scope,
    curr_rate: f64,
    min_rtt_us: u32,
    last_rate: f64,
    last_utility: f64
}

#[derive(Clone)]
pub struct PccConfig {
    pub logger: Option<slog::Logger>,
}

impl<T: Ipc> Pcc<T> {
    fn update_rate(&self) {
        let calculated_cwnd = (self.curr_rate * 2.0 * f64::from(self.min_rtt_us) / 1e6) as u32;
        self.control_channel
            .update_field(&self.sc, &[("Cwnd", calculated_cwnd), ("Rate", self.curr_rate)])
            .unwrap()
    }

    fn install_update(&self, update: &[(&str, u32)]) {
        if let Err(e) = self.control_channel.update_field(&self.sc, update) {
            self.logger.as_ref().map(|log| {
                warn!(log, "Cwnd and rate update error";
                      "err" => ?e,
                );
            });
        }
    }

    fn get_single_mi_report_fields(&mut self, m: &Report) -> Option<(u32, u32, u32, u32, u32, u32, u32, u32, f64)> {
        let ackedl = m
            .get_field(&String::from("Report.ackedl"), &self.sc)
            .expect("expected ackedl field in returned measurement") as u32;
        let lossl = m
            .get_field(&String::from("Report.lossl"), &self.sc)
            .expect("expected lossl field in returned measurement") as u32;
        let ackedr = m
            .get_field(&String::from("Report.ackedr"), &self.sc)
            .expect("expected ackedr field in returned measurement") as u32;
        let lossr = m
            .get_field(&String::from("Report.lossr"), &self.sc)
            .expect("expected lossr field in returned measurement") as u32;
        let sumrttl = m
            .get_field(&String::from("Report.sumrttl"), &self.sc)
            .expect("expected sumrttl field in returned measurement") as u32;
        let numrttl = m
            .get_field(&String::from("Report.numrttl"), &self.sc)
            .expect("expected numrttl field in returned measurement") as u32;
        let sumrttr = m
            .get_field(&String::from("Report.sumrttr"), &self.sc)
            .expect("expected sumrttr field in returned measurement") as u32;
        let numrttr = m
            .get_field(&String::from("Report.numrttr"), &self.sc)
            .expect("expected numrttr field in returned measurement") as u32;
        let sendrate = m
            .get_field(&String::from("Report.sendrate"), &self.sc)
            .expect("expected sendrate field in returned measurement") as f64;
        Some((ackedl, lossl, ackedr, lossr, sumrttl, numrttl, sumrttr, numrttr, sendrate))
    }
}

impl<T: Ipc> CongAlg<T> for PccConfig {
    type Flow = Pcc<T>;

    fn name() -> &'static str {
        "pcc"
    }

    fn datapath_programs(&self) -> HashMap<&'static str, String> {
        vec![
            (
                "single_mi_rate_control",
                String::from(
                    "
                (def
                    (Report
                        (ackedl 0)
                        (lossl 0)
                        (ackedr 0)
                        (lossr 0)
                        (sumrttl 0)
                        (numrttl 0)
                        (sumrttr 0)
                        (numrttr 0)
                        (sendrate 0)
                    )
                    (intervalState 0)
                    (totalAckedPkts 0)
                    (totalLostPkts 0)
                    (minrtt +infinity)
                    (startPktsInFlight 0)
                    (pacingRate 0)
                )
                (when true
                    (:= totalAckedPkts (+ totalAckedPkts Ack.packets_acked))
                    (:= totalLostPkts (+ totalLostPkts Ack.lost_pkts_sample))
                    (:= minrtt (min minrtt Flow.rtt_sample_us))
                    (fallthrough)
                )
                (when (== intervalState 0)
                    (:= intervalState 1)
                    (:= totalAckedPkts 0)
                    (:= totalLostPkts 0)
                    (:= startPktsInFlight Flow.packets_in_flight)
                    (:= Rate pacingRate)
                )
                (when (&& (== intervalState 1)
                          (> (+ totalAckedPkts totalLostPkts) startPktsInFlight))
                    (:= Report.ackedl 0)
                    (:= Report.lossl 0)
                    (:= Report.ackedr 0)
                    (:= Report.lossr 0)
                    (:= Report.sumrttl 0)
                    (:= Report.numrttl 0)
                    (:= Report.sumrttr 0)
                    (:= Report.numrttr 0)
                    (:= Report.sendrate 0)
                    (:= intervalState 2)
                    (:= Micros 0)
                )
                (when (== intervalState 2)
                    (:= Report.ackedl (+ Report.ackedl Ack.packets_acked))
                    (:= Report.lossl (+ Report.lossl Ack.lost_pkts_sample))
                    (:= Report.sumrttl (+ Report.sumrttl Flow.rtt_sample_us))
                    (:= Report.numrttl (+ Report.numrttl 1))
                    (fallthrough)
                )
                (when (&& (> Micros minrtt) (== intervalState 2))
                    (:= intervalState 3)
                )
                (when (== intervalState 3)
                    (:= Report.ackedr (+ Report.ackedr Ack.packets_acked))
                    (:= Report.lossr (+ Report.lossr Ack.lost_pkts_sample))
                    (:= Report.sumrttr (+ Report.sumrttr Flow.rtt_sample_us))
                    (:= Report.numrttr (+ Report.numrttr 1))
                    (fallthrough)
                )
                (when (&& (== intervalState 3)
                          (|| (< (+ Report.ackedl Report.lossl) (+ Report.ackedr Report.lossr))
                              (== (+ Report.ackedl Report.lossl) (+ Report.ackedr Report.lossr))))
                    (:= intervalState 4)
                    (:= Report.sendrate Flow.rate_outgoing)
                    (report)
                )
                    ",
                ),
            ),
        ]
        .into_iter()
        .collect()
    }

    fn new_flow(&self, control: Datapath<T>, info: DatapathInfo) -> Self::Flow {
        let mut s = Pcc {
            control_channel: control,
            logger: self.logger.clone(),
            sc: Default::default(),
            curr_rate: 125_000.0,
            min_rtt_us: 1_000_000,
            last_rate: 0.0,
            last_utility: 0.0,
        };

        s.sc = s
            .control_channel
            .set_program("single_mi_rate_control", Some(&[("Cwnd", info.init_cwnd)]))
            .unwrap();
        s.update_rate();
        s
    }
}

impl<T: Ipc> portus::Flow for Pcc<T> {
    fn on_report(&mut self, _sock_id: u32, m: Report) {
        //
    }
}