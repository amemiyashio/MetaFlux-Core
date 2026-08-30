# Notes

The cdev backpressure fixture uses two-slot rings and drains the pre-existing
FIFO entries before asserting the newly published completion. The vfio-user
socketpair keeps malformed packets local to framing so a following valid
GET_INFO request proves the server remains usable.
