#include "metrics.h"
SystemMetrics g_systemMetrics;
MetricsView g_metricsView(&g_systemMetrics);
thread_local MetricsView::Local MetricsView::local;

static void metricsAtExit() {
	g_metricsView.flush();
}

__attribute__((constructor)) static void registerMetricsExit() {
	std::atexit(metricsAtExit);
}

void flushAllMetrics() noexcept {
	g_metricsView.flush();
}
