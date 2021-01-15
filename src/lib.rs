#[macro_use]
extern crate slog;
extern crate fnv;
extern crate portus;
extern crate time;

use portus::ipc::Ipc;
use portus::lang::Scope;
use portus::{CongAlg, Datapath, DatapathInfo, DatapathTrait, Report};
use std::collections::HashMap;
use std::cmp;

pub struct Pcc<T: Ipc> {
    control_channel: Datapath<T>,
    logger: Option<slog::Logger>,
    sc: Scope,
    curr_rate: f64,
    min_rtt_us: u32,
    last_rate: f64,
    last_utility: f64,
    last_dir: i32,
    dir_rounds: u32;
    initial_max_step_size: f64,
    incremental_step_size: f64,
    incremental_steps: u32
}

#[derive(Clone)]
pub struct PccConfig {
    pub logger: Option<slog::Logger>,
}

impl<T: Ipc> Pcc<T> {
    fn update_rate(&self) {
        let calculated_cwnd = (self.curr_rate * 2.0 * f64::from(self.min_rtt_us) / 1e6) as u32;
        self.control_channel
            .update_field(
                &self.sc,
                &[
                    ("intervalState", 0)
                    ("Cwnd", calculated_cwnd),
                    ("Rate", self.curr_rate as u32),
                ])
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
        let minrtt = m
            .get_field(&String::from("Report.minrtt"), &self.sc)
            .expect("expected minrtt field in returned measurement") as u32;
        let sendrate = m
            .get_field(&String::from("Report.sendrate"), &self.sc)
            .expect("expected sendrate field in returned measurement") as f64;
        Some((ackedl, lossl, ackedr, lossr, sumrttl, numrttl, sumrttr, numrttr, minrtt, sendrate))
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
                        (volatile ackedl 0)
                        (volatile lossl 0)
                        (volatile ackedr 0)
                        (volatile lossr 0)
                        (volatile sumrttl 0)
                        (volatile numrttl 0)
                        (volatile sumrttr 0)
                        (volatile numrttr 0)
                        (minrtt +infinity)
                        (volatile sendrate 0)
                    )
                    (intervalState 0)
                    (totalAckedPkts 0)
                    (totalLostPkts 0)
                    (startPktsInFlight 0)
                    (pacingRate 0)
                )
                (when true
                    (:= totalAckedPkts (+ totalAckedPkts Ack.packets_acked))
                    (:= totalLostPkts (+ totalLostPkts Ack.lost_pkts_sample))
                    (:= Report.minrtt (min Report.minrtt Flow.rtt_sample_us))
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
                (when (&& (> Micros Report.minrtt) (== intervalState 2))
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
            last_dir: 0,
            dir_rounds: 0,
            initial_max_step_size: 0.05,
            incremental_step_size: 0.05,
            incremental_steps: 0
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
        let fields = self.get_single_mi_report_fields(&m);
        if fields.is_none() {
            return;
        }

        let (ackedl, lossl, ackedr, lossr, sumrttl, numrttl, sumrttr, numrttr, minrtt, sendrate) = fields.unwrap();
        if ackedr + ackedl + lossl + lossr == 0 {
            return;
        }

        self.min_rtt_us = minrtt;
        self.logger.as_ref().map(|log| {
            info!(log, "Get Report";
                "acked (first half)" => ackedl,
                "loss (first half)" => lossl,
                "acked (second half)" => ackedr,
                "loss (second half)" => lossr,
                "avg rtt (first half)" => sumrttr / numrttl,
                "avg rtt (second half)" => sumrttr / numrttr,
                "min rtt" => minrtt,
                "send rate (Mbps)" => sendrate / 125_000.0,
            );
        });

        let rate_mbps = sendrate / 125_000.0;
        let utility_send_rate = rate_mbps.powf(0.9);

        let avg_rtt_left = (sumrttl as f64 / numrttl as f64);
        let avg_rtt_right = (sumrttr as f64 / numrttr as f64);
        let avg_rtt = 0.5 * (avg_rtt_left + avg_rtt_right);
        let rtt_grad_approx = (avg_rtt_right - avg_rtt_left) / avg_rtt;
        let utility_rtt_grad = 900.0 * rate_mbps * rtt_grad_approx;

        let acked_total = (ackedl + ackedr) as f64;
        let loss_total = (lossl + lossr) as f64;
        let loss_rate = loss_total / (acked_total + loss_total);
        let utility_loss = 11.35 * rate_mbps * loss_rate;

        let utility = utility_send_rate - utility_rtt_grad - utility_loss;

        if self.last_rate < 1e-10 {
            self.last_rate = rate_mbps;
            self.last_utility = utility;
            self.last_dir = 1;
            self.dir_rounds = 1;
            self.curr_rate *= 2.;

            self.update_rate();
            return;
        }

        let delta_utility = (utility - self.last_utility).abs();
        let delta_rate = (rate_mbps - self.last_rate).abs();
        let rate_change = delta_utility / delta_rate;

        let direction = -1i32;
        if (utility > self.last_utility && rate_mbps > self.last_rate)
            || (utility < self.last_utility && rate_mbps < self.last_rate) {
            direction = 1i32;
        }

        if direction != self.last_dir {
            self.dir_rounds = 1;
            self.incremental_steps = 0;
        } else {
            self.dir_rounds += 1;
            rate_change = rate_change * ((1 + self.dir_rounds as f64) * 0.5).powf(1.2);
        }

        let max_rate_change = rate_mbps * (self.initial_max_step_size + self.incremental_step_size * (self.incremental_steps as f64));
        if rate_change > max_step_size {
            rate_change = max_rate_change;
            self.incremental_steps = self.incremental_steps + 1;
        } else {
            self.incremental_steps = cmp::max(self.incremental_steps, self.incremental_steps - 1);
        }

        self.curr_rate = (rate_mbps + (direction as f64) * rate_change) * 125_000.0;
        self.last_rate = rate_mbps;
        self.last_utility = utility;
        self.last_dir = direction;

        self.update_rate();
    }
}
