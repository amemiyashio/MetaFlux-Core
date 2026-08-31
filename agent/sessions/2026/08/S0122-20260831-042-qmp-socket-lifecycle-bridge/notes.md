# Notes

The bridge deliberately returns transport and lifecycle outcomes separately;
callers can route a closed QMP socket to the existing Disconnect handoff rather
than treating a transport error as a lifecycle commit. Wrong-kind events are
consumed as non-matching replies and leave the adapter pending.
