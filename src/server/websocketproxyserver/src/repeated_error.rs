//! Bounded diagnostics, with fixed state allocated from configuration.

use std::collections::HashMap;
use std::sync::Mutex;
use std::time::{Duration, Instant};

const REPORT_INTERVAL: Duration = Duration::from_secs(30);

#[derive(Debug, Default)]
pub(crate) struct RepeatedError {
    last_report: Option<Instant>,
    suppressed: u64,
    reported_failure: bool,
}

impl RepeatedError {
    pub(crate) fn failure(&mut self, now: Instant) -> Option<u64> {
        if self
            .last_report
            .is_none_or(|last| now.saturating_duration_since(last) >= REPORT_INTERVAL)
        {
            self.last_report = Some(now);
            self.reported_failure = true;
            Some(self.take_suppressed())
        } else {
            self.suppressed = self.suppressed.saturating_add(1);
            None
        }
    }

    // Recovery never replenishes the emission budget. Repeated brief failures
    // and successes are counted without a warning/recovery pair per request.
    pub(crate) fn recovery(&mut self) -> Option<u64> {
        if !self.reported_failure {
            return None;
        }
        self.reported_failure = false;
        Some(self.take_suppressed())
    }

    pub(crate) fn take_suppressed(&mut self) -> u64 {
        std::mem::take(&mut self.suppressed)
    }
}

#[derive(Debug)]
pub(crate) struct BackendReports(Mutex<HashMap<u16, RepeatedError>>);

impl BackendReports {
    pub(crate) fn new(ports: impl Iterator<Item = u16>) -> Self {
        Self(Mutex::new(
            ports.map(|port| (port, RepeatedError::default())).collect(),
        ))
    }

    pub(crate) fn failure(&self, port: u16, now: Instant) -> Option<u64> {
        self.0
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner)
            .get_mut(&port)?
            .failure(now)
    }

    pub(crate) fn recovery(&self, port: u16) -> Option<u64> {
        self.0
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner)
            .get_mut(&port)?
            .recovery()
    }

    // Called only after connection tasks have finished or been aborted.
    pub(crate) fn finish(&self) {
        for (backend_port, report) in self
            .0
            .lock()
            .unwrap_or_else(std::sync::PoisonError::into_inner)
            .iter_mut()
        {
            let suppressed_failures = report.take_suppressed();
            if suppressed_failures != 0 {
                tracing::warn!(
                    backend_port,
                    suppressed_failures,
                    "backend failures suppressed before shutdown"
                );
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn reports_first_failure_and_exact_window_count() {
        let now = Instant::now();
        let mut report = RepeatedError::default();
        assert_eq!(report.failure(now), Some(0));
        for _ in 0..10_000 {
            assert_eq!(
                report.failure(now + REPORT_INTERVAL - Duration::from_nanos(1)),
                None
            );
        }
        assert_eq!(report.failure(now + REPORT_INTERVAL), Some(10_000));
        assert_eq!(report.failure(now + REPORT_INTERVAL), None);
        assert_eq!(report.take_suppressed(), 1);
        assert_eq!(report.take_suppressed(), 0);
    }

    #[test]
    fn recovery_cannot_bypass_the_budget() {
        let now = Instant::now();
        let mut report = RepeatedError::default();
        assert_eq!(report.recovery(), None);
        assert_eq!(report.failure(now), Some(0));
        assert_eq!(report.failure(now), None);
        assert_eq!(report.recovery(), Some(1));
        for _ in 0..10_000 {
            assert_eq!(report.failure(now), None);
            assert_eq!(report.recovery(), None);
        }
        assert_eq!(report.failure(now + REPORT_INTERVAL), Some(10_000));
        assert_eq!(report.recovery(), Some(0));
    }

    #[test]
    fn backend_budgets_are_shared_per_configured_port_and_never_grow() {
        let now = Instant::now();
        let reports = BackendReports::new([19099, 19099, 19100].into_iter());
        assert_eq!(reports.failure(19099, now), Some(0));
        assert_eq!(reports.failure(19099, now), None);
        assert_eq!(reports.failure(19100, now), Some(0));
        for port in 1..19099 {
            assert_eq!(reports.failure(port, now), None);
            assert_eq!(reports.recovery(port), None);
        }
        assert_eq!(reports.0.lock().unwrap().len(), 2);
        assert_eq!(reports.recovery(19099), Some(1));
        assert_eq!(reports.recovery(19100), Some(0));
    }

    #[test]
    fn suppressed_counter_saturates() {
        let now = Instant::now();
        let mut report = RepeatedError {
            last_report: Some(now),
            suppressed: u64::MAX,
            reported_failure: true,
        };
        assert_eq!(report.failure(now), None);
        assert_eq!(report.recovery(), Some(u64::MAX));
    }
}
