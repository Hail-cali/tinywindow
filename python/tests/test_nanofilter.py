"""Basic tests for Python bindings (requires libnanofilter built)."""

import pytest
from tinywindow import TimingCMS, SlidingBloomFilter


class TestTimingCMS:
    def test_basic_count(self):
        with TimingCMS(depth=5, width=10000, num_slots=12, slot_duration_ms=300000) as cms:
            now = 1_000_000
            for _ in range(5):
                cms.record("user:abc", now_ms=now)
            count = cms.count("user:abc", window_ms=3_600_000, now_ms=now)
            assert count >= 5

    def test_unknown_key(self):
        with TimingCMS(depth=5, width=10000, num_slots=12, slot_duration_ms=300000) as cms:
            now = 1_000_000
            cms.record("user:abc", now_ms=now)
            count = cms.count("user:unknown", window_ms=3_600_000, now_ms=now)
            assert count == 0

    def test_reset(self):
        with TimingCMS(depth=4, width=1000, num_slots=6, slot_duration_ms=10000) as cms:
            now = 1_000_000
            for _ in range(10):
                cms.record("key", now_ms=now)
            assert cms.count("key", window_ms=60000, now_ms=now) >= 10
            cms.reset()
            assert cms.count("key", window_ms=60000, now_ms=now) == 0


class TestSlidingBloomFilter:
    def test_basic(self):
        with SlidingBloomFilter(expected_items=10000, fp_rate=0.01,
                                num_slots=6, slot_duration_ms=10000) as bf:
            now = 1_000_000
            bf.insert("click:abc123", now_ms=now)
            assert bf.might_contain("click:abc123", window_ms=60000, now_ms=now) is True
            assert bf.might_contain("click:never", window_ms=60000, now_ms=now) is False

    def test_expiry(self):
        with SlidingBloomFilter(expected_items=10000, fp_rate=0.01,
                                num_slots=6, slot_duration_ms=10000) as bf:
            t = 1_000_000
            bf.insert("click:abc123", now_ms=t)
            t2 = t + 6 * 10000
            assert bf.might_contain("click:abc123", window_ms=60000, now_ms=t2) is False
