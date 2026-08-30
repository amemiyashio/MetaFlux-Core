# Notes

The current coordinator uses fixed arrays for replay records and immutable
tombstones. Since old generations must resolve as `DeviceLost`, this session
does not add an eviction policy. The long-cycle fixture will use terminal values
above the calculated high-water marks and verify every retired generation.
