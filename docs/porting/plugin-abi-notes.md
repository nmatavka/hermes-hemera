# Haiku Plugin ABI Notes

The Haiku port does not preserve Windows DLL binary compatibility.
The compatibility target is source-compatible, EMS-style plugins rebuilt as native Haiku `.so` modules.

## Current portable ABI surface

- Descriptor entry point: `HermesPlugin_GetDescriptor`
- Capability flags:
  - `kPluginCapabilityMenu`
  - `kPluginCapabilityTranslator`
  - `kPluginCapabilitySpecial`
  - `kPluginCapabilitySecurity`

## Porting rule

Existing plugin code should move behind a thin host adaptation layer:

- Windows loader code is replaced by the portable `FilesystemPluginHost`.
- Plugin descriptors stay small and app-owned so the ABI can be verified at load time.
- Message translators, specials, and security handlers stay separate capability families rather than being collapsed into one generic hook.
