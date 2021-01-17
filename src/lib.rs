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
    curr_mode: PccMode,
    four_mi_utility_count: u32,
    four_mi_utility1: f64,
    four_mi_utility2: f64,
    four_mi_utility3: f64,
    four_mi_utility4: f64,
    curr_rate: f64,
    min_rtt_us: u32,
    smoothed_rtt_us: u32,
    last_rate: f64,
    last_avg_rtt: u32,
    last_utility: f64,
    last_dir: i32,
    dir_rounds: u32,
    min_rate: f64,
    max_step_size: f64,
    initial_max_step_size: f64,
    incremental_step_size: f64,
    incremental_steps: u32
}

enum PccMode {
    SingleInterval,
    FourInterval,
}

#[derive(Clone)]
pub struct PccConfig {
    pub logger: Option<slog::Logger>,
}

impl<T: Ipc> Pcc<T> {
    fn update_rate_single_interval(&self) {
        let calculated_cwnd = (self.curr_rate * 2.0 * f64::from(self.min_rtt_us) / 1e6) as u32;
        self.control_channel
            .update_field(
                &self.sc,
                &[
                    ("srtt", self.smoothed_rtt_us),
                    ("intervalState", 0),
                    ("Cwnd", calculated_cwnd),
                    ("Rate", self.curr_rate as u32),
                ])
            .unwrap();

        self.logger.as_ref().map(|log| {
            info!(log, "Update Rate";
                "srtt" => self.smoothed_rtt_us,
                "cwnd" => calculated_cwnd,
                "new send rate (Mbps)" => self.curr_rate / 125_000.0,
            );
        });
    }

    fn update_rate_four_interval(&self) {
        let calculated_cwnd = (self.curr_rate * 2.0 * f64::from(self.min_rtt_us) / 1e6) as u32;
        let rate_higher = (self.curr_rate * 1.05) as u32;
        let rate_lower = (self.curr_rate * 0.95) as u32;

        self.control_channel
            .update_field(
                &self.sc,
                &[
                    ("txIntervalIndex", 0),
                    ("srtt", self.smoothed_rtt_us),
                    ("pacingRateLower", rate_lower),
                    ("pacingRateHigher", rate_higher),
                    ("Cwnd", calculated_cwnd),
                    ("Rate", rate_lower),
                ])
            .unwrap();

        self.logger.as_ref().map(|log| {
            info!(log, "Update Rate";
                "srtt" => self.smoothed_rtt_us,
                "cwnd" => calculated_cwnd,
                "new central probing rate (Mbps)" => self.curr_rate / 125_000.0,
            );
        });
    }

    fn get_single_mi_report_fields(&mut self, m: &Report) -> Option<(u32, u32, u32, u32, u32, u32, u32, u32, u32, u32, u32)> {
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
        let srttstart = m
            .get_field(&String::from("Report.srttstart"), &self.sc)
            .expect("expected srttstart field in returned measurement") as u32;
        let srttend = m
            .get_field(&String::from("Report.srttend"), &self.sc)
            .expect("expected srttend field in returned measurement") as u32;
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
        Some((ackedl, lossl, ackedr, lossr, srttstart, srttend, sumrttl, numrttl, sumrttr, numrttr, minrtt))
    }

    fn get_four_mi_report_fields(&mut self, m: &Report) -> Option<(u32, u32, u32, u32, u32, u32, u32, u32, u32)> {
        let acked = m
            .get_field(&String::from("Report.acked"), &self.sc)
            .expect("expected acked field in returned measurement") as u32;
        let loss = m
            .get_field(&String::from("Report.loss"), &self.sc)
            .expect("expected loss field in returned measurement") as u32;
        let srttstart = m
            .get_field(&String::from("Report.srttstart"), &self.sc)
            .expect("expected srttstart field in returned measurement") as u32;
        let srttend = m
            .get_field(&String::from("Report.srttend"), &self.sc)
            .expect("expected srttend field in returned measurement") as u32;
        let sumrtt = m
            .get_field(&String::from("Report.sumrtt"), &self.sc)
            .expect("expected sumrtt field in returned measurement") as u32;
        let numrtt = m
            .get_field(&String::from("Report.numrtt"), &self.sc)
            .expect("expected numrtt field in returned measurement") as u32;
        let minrtt = m
            .get_field(&String::from("Report.minrtt"), &self.sc)
            .expect("expected minrtt field in returned measurement") as u32;
        let probingratechoice = m
            .get_field(&String::from("Report.probingratechoice"), &self.sc)
            .expect("expected probingratechoice field in returned measurement") as u32;
        let countacks = m
            .get_field(&String::from("Report.countacks"), &self.sc)
            .expect("expected countacks field in returned measurement") as u32;
        Some((acked, loss, srttstart, srttend, sumrtt, numrtt, minrtt, probingratechoice, countacks))
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
                        (volatile srttstart 0)
                        (volatile srttend 0)
                        (volatile sumrttl 0)
                        (volatile numrttl 0)
                        (volatile sumrttr 0)
                        (volatile numrttr 0)
                        (minrtt +infinity)
                    )
                    (srtt 0)
                    (intervalState 0)
                    (totalAckedPkts 0)
                    (totalLostPkts 0)
                    (startPktsInFlight 0)
                    (pacingRate 0)
                )
                (when (== srtt 0)
                    (:= srtt Flow.rtt_sample_us)
                )
                (when true
                    (:= totalAckedPkts (+ totalAckedPkts Ack.packets_acked))
                    (:= totalLostPkts (+ totalLostPkts Ack.lost_pkts_sample))
                    (:= Report.minrtt (min Report.minrtt Flow.rtt_sample_us))
                    (fallthrough)
                )
                (when (> srtt 0)
                    (:= srtt (/ (+ (* srtt 9) (* Flow.rtt_sample_us 1)) 10))
                )
                (when (== intervalState 0)
                    (:= intervalState 1)
                    (:= totalAckedPkts 0)
                    (:= totalLostPkts 0)
                    (:= startPktsInFlight Flow.packets_in_flight)
                    (:= Rate pacingRate)
                )
                (when (== Report.srttstart 0)
                    (:= Report.srttstart srtt)
                    (fallthrough)
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
                (when (&& (> (* Micros 10) (* Report.minrtt 6)) (== intervalState 2))
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
                    (:= Report.srttend srtt)
                    (report)
                )
                    ",
                ),
            ),
            (
                "four_mi_rate_control",
                String::from(
                    "
                (def
                    (Report
                        (volatile acked 0)
                        (volatile loss 0)
                        (volatile srttstart 0)
                        (volatile srttend 0)
                        (volatile sumrtt 0)
                        (volatile numrtt 0)
                        (minrtt +infinity)
                        (volatile probingratechoice 0)
                        (volatile countacks 0)
                    )
                    (srtt 0)
                    (pacingRateLower 0)
                    (pacingRateHigher 0)
                    (txIntervalIndex 0)
                    (rxIntervalIndex 0)
                    (pktsReceivedTotal 0)
                    (pktsReceivedSnapshot1 0)
                    (pktsInFlight1 0)
                    (pktsReceivedSnapshot2 0)
                    (pktsInFlight2 0)
                    (pktsReceivedSnapshot3 0)
                    (pktsInFlight3 0)
                    (pktsReceivedSnapshot4 0)
                    (pktsInFlight4 0)
                    (pktsReceivedSnapshot5 0)
                    (pktsInFlight5 0)
                )
                (when true
                    (:= pktsReceivedTotal (+ (+ pktsReceivedTotal Ack.packets_acked) Ack.lost_pkts_sample))
                    (:= Report.minrtt (min Report.minrtt Flow.rtt_sample_us))
                    (:= Report.countacks (+ Report.countacks 1))
                    (fallthrough)
                )
                (when (> srtt 0)
                    (:= srtt (/ (+ (* srtt 9) (* Flow.rtt_sample_us 1)) 10))
                )
                (when (== txIntervalIndex 0)
                    (:= txIntervalIndex 1)
                    (:= rxIntervalIndex 0)
                    (:= pktsReceivedTotal 0)
                    (:= pktsReceivedSnapshot1 0)
                    (:= pktsReceivedSnapshot2 0)
                    (:= pktsReceivedSnapshot3 0)
                    (:= pktsReceivedSnapshot4 0)
                    (:= pktsReceivedSnapshot5 0)
                    (:= pktsInFlight1 (+ Flow.packets_in_flight Ack.packets_misordered))
                    (:= pktsInFlight2 0)
                    (:= pktsInFlight3 0)
                    (:= pktsInFlight4 0)
                    (:= pktsInFlight5 0)
                    (:= Rate pacingRateLower)
                    (:= Micros 0)
                )
                (when (&& (== txIntervalIndex 1) (> Micros Report.minrtt))
                    (:= txIntervalIndex 2)
                    (:= pktsReceivedSnapshot2 pktsReceivedTotal)
                    (:= pktsInFlight2 (+ Flow.packets_in_flight Ack.packets_misordered))
                    (:= Rate pacingRateHigher)
                    (:= Micros 0)
                    (fallthrough)
                )
                (when (&& (== txIntervalIndex 2) (> Micros Report.minrtt))
                    (:= txIntervalIndex 3)
                    (:= pktsReceivedSnapshot3 pktsReceivedTotal)
                    (:= pktsInFlight3 (+ Flow.packets_in_flight Ack.packets_misordered))
                    (:= Rate pacingRateLower)
                    (:= Micros 0)
                    (fallthrough)
                )
                (when (&& (== txIntervalIndex 3) (> Micros Report.minrtt))
                    (:= txIntervalIndex 4)
                    (:= pktsReceivedSnapshot4 pktsReceivedTotal)
                    (:= pktsInFlight4 (+ Flow.packets_in_flight Ack.packets_misordered))
                    (:= Rate pacingRateHigher)
                    (:= Micros 0)
                    (fallthrough)
                )
                (when (&& (== txIntervalIndex 4) (> Micros Report.minrtt))
                    (:= txIntervalIndex 5)
                    (:= pktsReceivedSnapshot5 pktsReceivedTotal)
                    (:= pktsInFlight5 (+ Flow.packets_in_flight Ack.packets_misordered))
                    (:= Rate (/ (+ pacingRateLower pacingRateHigher) 2))
                    (fallthrough)
                )
                (when (&& (== rxIntervalIndex 0) (> pktsReceivedTotal (+ pktsReceivedSnapshot1 pktsInFlight1)))
                    (:= rxIntervalIndex 1)
                )
                (when (== Report.srttstart 0)
                    (:= Report.srttstart srtt)
                    (fallthrough)
                )
                (when (== rxIntervalIndex 1)
                    (:= Report.acked (+ Report.acked Ack.packets_acked))
                    (:= Report.loss (+ Report.loss Ack.lost_pkts_sample))
                    (:= Report.sumrtt (+ Report.sumrtt Flow.rtt_sample_us))
                    (:= Report.numrtt (+ Report.numrtt 1))
                    (fallthrough)
                )
                (when (&& (== rxIntervalIndex 1) (> pktsReceivedTotal (+ pktsReceivedSnapshot2 pktsInFlight2)))
                    (:= Report.acked (- Report.acked Ack.packets_acked))
                    (:= Report.loss (- Report.loss Ack.lost_pkts_sample))
                    (:= Report.sumrtt (- Report.sumrtt Flow.rtt_sample_us))
                    (:= Report.numrtt (- Report.numrtt 1))
                    (:= Report.probingratechoice 1)
                    (:= Report.srttend srtt)
                    (:= rxIntervalIndex 2)
                    (report)
                )
                (when (== rxIntervalIndex 2)
                    (:= Report.acked (+ Report.acked Ack.packets_acked))
                    (:= Report.loss (+ Report.loss Ack.lost_pkts_sample))
                    (:= Report.sumrtt (+ Report.sumrtt Flow.rtt_sample_us))
                    (:= Report.numrtt (+ Report.numrtt 1))
                    (fallthrough)
                )
                (when (&& (== rxIntervalIndex 2) (> pktsReceivedTotal (+ pktsReceivedSnapshot3 pktsInFlight3)))
                    (:= Report.acked (- Report.acked Ack.packets_acked))
                    (:= Report.loss (- Report.loss Ack.lost_pkts_sample))
                    (:= Report.sumrtt (- Report.sumrtt Flow.rtt_sample_us))
                    (:= Report.numrtt (- Report.numrtt 1))
                    (:= Report.probingratechoice 2)
                    (:= Report.srttend srtt)
                    (:= rxIntervalIndex 3)
                    (report)
                )
                (when (== rxIntervalIndex 3)
                    (:= Report.acked (+ Report.acked Ack.packets_acked))
                    (:= Report.loss (+ Report.loss Ack.lost_pkts_sample))
                    (:= Report.sumrtt (+ Report.sumrtt Flow.rtt_sample_us))
                    (:= Report.numrtt (+ Report.numrtt 1))
                    (fallthrough)
                )
                (when (&& (== rxIntervalIndex 3) (> pktsReceivedTotal (+ pktsReceivedSnapshot4 pktsInFlight4)))
                    (:= Report.acked (- Report.acked Ack.packets_acked))
                    (:= Report.loss (- Report.loss Ack.lost_pkts_sample))
                    (:= Report.sumrtt (- Report.sumrtt Flow.rtt_sample_us))
                    (:= Report.numrtt (- Report.numrtt 1))
                    (:= Report.probingratechoice 1)
                    (:= Report.srttend srtt)
                    (:= rxIntervalIndex 4)
                    (report)
                )
                (when (== rxIntervalIndex 4)
                    (:= Report.acked (+ Report.acked Ack.packets_acked))
                    (:= Report.loss (+ Report.loss Ack.lost_pkts_sample))
                    (:= Report.sumrtt (+ Report.sumrtt Flow.rtt_sample_us))
                    (:= Report.numrtt (+ Report.numrtt 1))
                    (fallthrough)
                )
                (when (&& (== rxIntervalIndex 4) (> pktsReceivedTotal (+ pktsReceivedSnapshot5 pktsInFlight5)))
                    (:= Report.acked (- Report.acked Ack.packets_acked))
                    (:= Report.loss (- Report.loss Ack.lost_pkts_sample))
                    (:= Report.sumrtt (- Report.sumrtt Flow.rtt_sample_us))
                    (:= Report.numrtt (- Report.numrtt 1))
                    (:= Report.probingratechoice 2)
                    (:= Report.srttend srtt)
                    (:= rxIntervalIndex 5)
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
            sc: Scope::new(),
            curr_mode: PccMode::SingleInterval,
            four_mi_utility_count: 0,
            four_mi_utility1: 0.0,
            four_mi_utility2: 0.0,
            four_mi_utility3: 0.0,
            four_mi_utility4: 0.0,
            curr_rate: 125_000.0,
            min_rtt_us: 1_000_000,
            smoothed_rtt_us: 1_000_000,
            last_rate: 0.0,
            last_avg_rtt: 0,
            last_utility: 0.0,
            last_dir: 0,
            dir_rounds: 0,
            min_rate: 125_000.0,
            max_step_size: 0.25,
            initial_max_step_size: 0.05,
            incremental_step_size: 0.05,
            incremental_steps: 0
        };

        s.sc = s
            .control_channel
            .set_program("single_mi_rate_control", Some(&[("Cwnd", info.init_cwnd)]))
            .unwrap();
        s.update_rate_single_interval();
        s
    }
}

impl<T: Ipc> portus::Flow for Pcc<T> {
    fn on_report(&mut self, _sock_id: u32, m: Report) {
        if self.sc.program_uid != m.program_uid {
            return;
        }

        let rate_mbps;
        let avg_rtt;
        let mut rtt_grad_approx;
        let acked_total;
        let loss_total;

        match self.curr_mode {
            PccMode::SingleInterval => {
                let fields = self.get_single_mi_report_fields(&m);
                if fields.is_none() {
                    return;
                }

                let (ackedl, lossl, ackedr, lossr, srttstart, srttend, sumrttl, numrttl, sumrttr, numrttr, minrtt) = fields.unwrap();
                if ackedr + ackedl + lossl + lossr == 0 {
                    return;
                }

                if (self.min_rtt_us == 1_000_000) || (self.min_rtt_us > minrtt) {
                    self.min_rtt_us = minrtt;
                }
                self.smoothed_rtt_us = srttend;
                self.logger.as_ref().map(|log| {
                    info!(log, "Get Report";
                        "loss-2" => lossr,
                        "acked-2" => ackedr,
                        "loss-1" => lossl,
                        "acked-1" => ackedl,
                        "srtt-end" => srttend,
                        "avg rtt-2" => sumrttr / numrttr,
                        "avg rtt-1" => sumrttl / numrttl,
                        "send rate (Mbps)" => self.curr_rate / 125_000.0,
                    );
                });

                rate_mbps = self.curr_rate / 125_000.0;

                let avg_rtt_left = sumrttl as f64 / numrttl as f64;
                let avg_rtt_right = sumrttr as f64 / numrttr as f64;
                avg_rtt = 0.5 * (avg_rtt_left + avg_rtt_right);
                rtt_grad_approx = (srttend as f64 - srttstart as f64) / avg_rtt;

                acked_total = (ackedl + ackedr) as f64;
                loss_total = (lossl + lossr) as f64;
            }
            PccMode::FourInterval => {
                let fields = self.get_four_mi_report_fields(&m);
                if fields.is_none() {
                    return;
                }

                let (acked, loss, srttstart, srttend, sumrtt, numrtt, minrtt, probingratechoice, countacks) = fields.unwrap();
                if acked + loss == 0 {
                    return;
                }

                if (self.min_rtt_us == 1_000_000) || (self.min_rtt_us > minrtt) {
                    self.min_rtt_us = minrtt;
                }
                self.smoothed_rtt_us = srttend;

                if probingratechoice == 1 {
                    rate_mbps = self.curr_rate * 0.95 / 125_000.0;
                } else {
                    rate_mbps = self.curr_rate * 1.05 / 125_000.0;
                }
                self.logger.as_ref().map(|log| {
                    info!(log, "Get Report";
                        "countacks" => countacks,
                        "loss" => loss,
                        "acked" => acked,
                        "srtt-end" => srttend,
                        "srtt-start" => srttstart,
                        "avg rtt" => sumrtt / numrtt,
                        "send rate (Mbps)" => rate_mbps,
                    );
                });

                avg_rtt = sumrtt as f64 / numrtt as f64;
                if self.last_avg_rtt == 0 {
                    self.last_avg_rtt = avg_rtt as u32;
                }
                //rtt_grad_approx = (avg_rtt - self.last_avg_rtt as f64) * 2.0 / (avg_rtt + self.last_avg_rtt as f64);
                rtt_grad_approx = (srttend as f64 - srttstart as f64) / self.min_rtt_us as f64;

                acked_total = acked as f64;
                loss_total = loss as f64;
            }
        }

        if rtt_grad_approx < 0.01 {
            // Negative RTT Gradient is ignored altogether.
            rtt_grad_approx = 0.0;
        }
        let loss_rate = loss_total / (acked_total + loss_total);

        let utility_send_rate = rate_mbps.powf(0.9);
        let utility_rtt_grad = 900.0 * rate_mbps * rtt_grad_approx;
        let utility_loss = 11.35 * rate_mbps * loss_rate;

        let utility = utility_send_rate - utility_rtt_grad - utility_loss;
        self.logger.as_ref().map(|log| {
            info!(log, "Calculated Utility";
                "Rtt Gradient" => rtt_grad_approx,
                "Utility" => utility,
            );
        });

        if self.last_rate < 1e-10 {
            self.last_rate = self.curr_rate;
            self.last_avg_rtt = avg_rtt as u32;
            self.last_utility = utility;
            self.last_dir = 1;
            self.dir_rounds = 1;
            self.curr_rate *= 2.;

            self.update_rate_single_interval();
            return;
        }

        match self.curr_mode {
            PccMode::SingleInterval => {
                let mut should_switch_to_four_interval: bool = false;

                let last_rate_mbps = self.last_rate / 125_000.0;
                let delta_utility = (utility - self.last_utility).abs();
                let delta_rate = (rate_mbps - last_rate_mbps).abs();
                let mut rate_change = delta_utility / delta_rate;

                let mut direction = -1i32;
                if (utility > self.last_utility && rate_mbps > last_rate_mbps)
                    || (utility < self.last_utility && rate_mbps < last_rate_mbps) {
                    direction = 1i32;
                }

                let max_inflated_rtt_us = std::cmp::max((1.2 * f64::from(self.min_rtt_us)) as u32, 2_000);
                if avg_rtt > max_inflated_rtt_us as f64  {
                    direction = -1i32;
                }

                if direction != self.last_dir {
                    self.dir_rounds = 1;
                    self.incremental_steps = 0;
                    should_switch_to_four_interval = true;
                } else {
                    self.dir_rounds += 1;
                    rate_change = rate_change * ((1.0 + self.dir_rounds as f64) * 0.5).powf(1.2);
                }

                let mut max_rate_change = rate_mbps * (self.initial_max_step_size + self.incremental_step_size * (self.incremental_steps as f64));
                let max_rate_change_bound = rate_mbps * self.max_step_size;
                if max_rate_change > max_rate_change_bound {
                    max_rate_change = max_rate_change_bound;
                }
                if rate_change > max_rate_change {
                    rate_change = max_rate_change;
                    self.incremental_steps = self.incremental_steps + 1;
                } else {
                    if self.incremental_steps > 0 {
                        self.incremental_steps = self.incremental_steps - 1;
                    }
                }

                self.last_avg_rtt = avg_rtt as u32;
                self.last_utility = utility;
                self.last_dir = direction;

                if self.curr_rate < self.min_rate {
                    self.curr_rate = self.min_rate;
                    self.last_rate = 0.0;
                    self.last_avg_rtt = 0;
                    self.last_dir = 0;
                    self.last_utility = 0.0;
                    self.dir_rounds = 0;
                    self.incremental_steps = 0;

                    self.update_rate_single_interval();
                } else if should_switch_to_four_interval {
                    self.curr_rate = self.last_rate;
                    self.four_mi_utility_count = 0;
                    self.four_mi_utility1 = 0.0;
                    self.four_mi_utility2 = 0.0;
                    self.four_mi_utility3 = 0.0;
                    self.four_mi_utility4 = 0.0;
                    self.curr_mode = PccMode::FourInterval;
                    self.sc = self.control_channel.set_program("four_mi_rate_control", None).unwrap();
                    self.update_rate_four_interval();
                } else {
                    self.last_rate = self.curr_rate;
                    self.curr_rate = (rate_mbps + (direction as f64) * rate_change) * 125_000.0;
                    self.update_rate_single_interval();
                }
            }
            PccMode::FourInterval => {
                self.last_avg_rtt = avg_rtt as u32;

                self.four_mi_utility_count += 1;
                if self.four_mi_utility_count == 1 {
                    self.four_mi_utility1 = utility;
                } else if self.four_mi_utility_count == 2 {
                    self.four_mi_utility2 = utility;
                } else if self.four_mi_utility_count == 3 {
                    self.four_mi_utility3 = utility;
                } else {
                    self.four_mi_utility4 = utility;
                }

                if self.four_mi_utility_count < 4 {
                    return;
                }

                let mut direction = 0i32;
                if (self.four_mi_utility1 < self.four_mi_utility2)
                    && (self.four_mi_utility3 < self.four_mi_utility4) {
                    direction = 1i32;
                } else if (self.four_mi_utility1 > self.four_mi_utility2)
                    && (self.four_mi_utility3 > self.four_mi_utility4) {
                    direction = -1i32;
                }

                if direction == 0 {
                    self.four_mi_utility_count = 0;
                    self.four_mi_utility1 = 0.0;
                    self.four_mi_utility2 = 0.0;
                    self.four_mi_utility3 = 0.0;
                    self.four_mi_utility4 = 0.0;

                    self.update_rate_four_interval();
                    return;
                }

                self.last_dir = direction;

                if direction == 1 {
                    self.last_rate = self.curr_rate * 1.05;
                    self.last_utility = self.four_mi_utility4;
                    self.curr_rate = self.last_rate * (1.0 + self.initial_max_step_size);
                } else {
                    self.last_rate = self.curr_rate * 0.95;
                    self.last_utility = self.four_mi_utility3;
                    self.curr_rate = self.last_rate * (1.0 - self.initial_max_step_size);
                }

                if self.curr_rate < self.min_rate {
                    self.curr_rate = self.min_rate;
                    self.last_rate = 0.0;
                    self.last_avg_rtt = 0;
                    self.last_dir = 0;
                    self.last_utility = 0.0;
                    self.dir_rounds = 0;
                    self.incremental_steps = 0;
                }

                self.curr_mode = PccMode::SingleInterval;
                self.sc = self.control_channel.set_program("single_mi_rate_control", None).unwrap();
                self.update_rate_single_interval();
            }
        }
    }
}
