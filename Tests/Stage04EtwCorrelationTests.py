import pathlib
import sys
import unittest
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "tools"))
from Correlate_Stage04_Etw import IntervalIndex, split_callback, require

class CorrelationTests(unittest.TestCase):
    def test_migration_and_nested_interrupt_union(self):
        running = [(50, 140, 0), (170, 250, 1)]
        irq = {0: IntervalIndex([(110, 125), (120, 130)]), 1: IntervalIndex([(180, 185)])}
        scheduled, interrupted, executing, descheduled = split_callback(running, [140, 250], irq, 100, 200)
        self.assertEqual((scheduled, interrupted, executing, descheduled), (70, 25, 45, 30))
    def test_interrupt_without_context_switch(self):
        result = split_callback([(0, 1000, 0)], [1000], {0: IntervalIndex([(200, 900)])}, 100, 1000)
        self.assertEqual(result, (900, 700, 200, 0))
    def test_clip_edges_and_unscheduled(self):
        self.assertEqual(split_callback([(0, 100, 0)], [100], {}, 100, 200), (0, 0, 0, 100))
    def test_invalid_coverage_is_rejected(self):
        with self.assertRaises(ValueError):
            split_callback([(0, 100, 0), (50, 150, 1)], [100, 150], {}, 0, 150)
        with self.assertRaises(ValueError):
            IntervalIndex([(20, 10)])
    def test_integrity_guard_survives_python_optimization(self):
        with self.assertRaises(ValueError):
            require(False, "lost ETW events")

if __name__ == "__main__":
    unittest.main()
