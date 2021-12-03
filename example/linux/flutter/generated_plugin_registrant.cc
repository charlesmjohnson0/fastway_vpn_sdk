//
//  Generated file. Do not edit.
//

// clang-format off

#include "generated_plugin_registrant.h"

#include <fy_vpn_sdk/fy_vpn_sdk_plugin.h>

void fl_register_plugins(FlPluginRegistry* registry) {
  g_autoptr(FlPluginRegistrar) fy_vpn_sdk_registrar =
      fl_plugin_registry_get_registrar_for_plugin(registry, "FyVpnSdkPlugin");
  fy_vpn_sdk_plugin_register_with_registrar(fy_vpn_sdk_registrar);
}
