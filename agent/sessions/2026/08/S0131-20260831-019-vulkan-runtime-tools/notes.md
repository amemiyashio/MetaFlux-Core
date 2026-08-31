# Notes

The current host exposes an AMD RADV device when the Mesa ICD is present, but
the existing `.#vulkan` shell only materializes the loader and tool binaries.
The optional runtime profile is a provisioning route for local Vulkan smoke
tests. Device selection, capability truth, and qualification remain owned by
M0130 and CTest.
