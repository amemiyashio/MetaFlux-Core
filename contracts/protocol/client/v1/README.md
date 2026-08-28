# Client Protocol v1

Home of the bootstrap provider/client protocol version and the later encoded
control negotiation. The current header is a boundary fixture. Future records
are little-endian, fixed-width, sized, versioned, and independent of CUDA, HIP,
or any backend implementation.
