#include "runtime_config.h"

RuntimeConfig& runtimeConfig() {
    static RuntimeConfig cfg; // zero-init then load once externally
    return cfg;
}
