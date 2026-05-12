"""
cffi bindings to nanofilter C core.

Phase 2 implementation — requires libnanofilter.so/dylib to be installed
or available in LD_LIBRARY_PATH / DYLD_LIBRARY_PATH.
"""

import time
from cffi import FFI

ffi = FFI()

ffi.cdef("""
    typedef struct nf_timing_cms nf_timing_cms_t;
    typedef struct nf_sliding_bf nf_sliding_bf_t;

    nf_timing_cms_t *nf_tcms_create(uint32_t depth, uint32_t width,
                                     uint32_t num_slots, uint64_t slot_duration_ms);
    void nf_tcms_destroy(nf_timing_cms_t *cms);
    int nf_tcms_record(nf_timing_cms_t *cms, const char *key, uint64_t now_ms);
    uint64_t nf_tcms_count(nf_timing_cms_t *cms, const char *key,
                            uint64_t window_ms, uint64_t now_ms);
    void nf_tcms_reset(nf_timing_cms_t *cms);
    size_t nf_tcms_memory_usage(const nf_timing_cms_t *cms);

    nf_sliding_bf_t *nf_sbf_create(uint64_t expected_items, double fp_rate,
                                    uint32_t num_slots, uint64_t slot_duration_ms);
    void nf_sbf_destroy(nf_sliding_bf_t *bf);
    int nf_sbf_insert(nf_sliding_bf_t *bf, const char *key, uint64_t now_ms);
    _Bool nf_sbf_might_contain(nf_sliding_bf_t *bf, const char *key,
                                uint64_t window_ms, uint64_t now_ms);
    size_t nf_sbf_memory_usage(const nf_sliding_bf_t *bf);
""")

# Load the shared library
try:
    _lib = ffi.dlopen("libnanofilter.so")
except OSError:
    try:
        _lib = ffi.dlopen("libnanofilter.dylib")
    except OSError:
        raise ImportError(
            "Could not load libnanofilter. "
            "Make sure it is built and available in your library path."
        )


def _now_ms() -> int:
    return int(time.time() * 1000)


class TimingCMS:
    """Time-windowed Count-Min Sketch."""

    def __init__(self, depth: int, width: int, num_slots: int, slot_duration_ms: int):
        self._handle = _lib.nf_tcms_create(depth, width, num_slots, slot_duration_ms)
        if self._handle == ffi.NULL:
            raise MemoryError("Failed to allocate TimingCMS")

    def record(self, key: str, now_ms: int | None = None) -> None:
        if now_ms is None:
            now_ms = _now_ms()
        _lib.nf_tcms_record(self._handle, key.encode(), now_ms)

    def count(self, key: str, window_ms: int, now_ms: int | None = None) -> int:
        if now_ms is None:
            now_ms = _now_ms()
        return _lib.nf_tcms_count(self._handle, key.encode(), window_ms, now_ms)

    def reset(self) -> None:
        _lib.nf_tcms_reset(self._handle)

    @property
    def memory_usage(self) -> int:
        return _lib.nf_tcms_memory_usage(self._handle)

    def close(self) -> None:
        if self._handle is not None:
            _lib.nf_tcms_destroy(self._handle)
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass


class SlidingBloomFilter:
    """Time-windowed Bloom Filter for deduplication."""

    def __init__(self, expected_items: int, fp_rate: float, num_slots: int, slot_duration_ms: int):
        self._handle = _lib.nf_sbf_create(expected_items, fp_rate, num_slots, slot_duration_ms)
        if self._handle == ffi.NULL:
            raise MemoryError("Failed to allocate SlidingBloomFilter")

    def insert(self, key: str, now_ms: int | None = None) -> None:
        if now_ms is None:
            now_ms = _now_ms()
        _lib.nf_sbf_insert(self._handle, key.encode(), now_ms)

    def might_contain(self, key: str, window_ms: int, now_ms: int | None = None) -> bool:
        if now_ms is None:
            now_ms = _now_ms()
        return _lib.nf_sbf_might_contain(self._handle, key.encode(), window_ms, now_ms)

    @property
    def memory_usage(self) -> int:
        return _lib.nf_sbf_memory_usage(self._handle)

    def close(self) -> None:
        if self._handle is not None:
            _lib.nf_sbf_destroy(self._handle)
            self._handle = None

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass
