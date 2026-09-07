# SPDX-License-Identifier: GPL-3.0-or-later
import pathlib
import subprocess
import sys
import tempfile
import unittest

FIXTURE = pathlib.Path(sys.argv.pop(1)).resolve()
DEBUG_MARKERS = ("direct trace", "direct debug", "diagnostic_debug", "macro debug")
INFO_MARKERS = ("direct info", "direct notice", "direct warning", "direct error", "direct critical", "macro info")


class LoggerFilterTests(unittest.TestCase):
    def outputs(self, mode):
        # GIVEN a separate logger process and isolated log file.
        with tempfile.TemporaryDirectory(prefix="merovingian-log-filter-") as directory:
            log_path = pathlib.Path(directory) / "fixture.log"
            # WHEN the child exits, both asynchronous writers have joined.
            result = subprocess.run([str(FIXTURE), str(log_path), mode], capture_output=True,
                                    text=True, check=False, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)
            return result.stdout, log_path.read_text(encoding="utf-8")

    def test_default_info_filters_direct_structured_and_macro_messages(self):
        for output in self.outputs("defaults"):
            # THEN info and higher survive; lower levels never reach either sink.
            for marker in INFO_MARKERS:
                self.assertIn(marker, output)
            for marker in DEBUG_MARKERS:
                self.assertNotIn(marker, output)

    def test_explicit_info_overrides_trace_default(self):
        for output in self.outputs("explicit"):
            for marker in INFO_MARKERS:
                self.assertIn(marker, output)
            for marker in DEBUG_MARKERS:
                self.assertNotIn(marker, output)

    def test_off_suppresses_even_critical_but_explicit_override_survives(self):
        for output in self.outputs("off"):
            self.assertIn("enabled warning", output)
            for marker in INFO_MARKERS + DEBUG_MARKERS:
                self.assertNotIn(marker, output)

    def test_debug_module_override_allows_debug_but_not_trace(self):
        for output in self.outputs("override"):
            for marker in INFO_MARKERS + DEBUG_MARKERS[1:]:
                self.assertIn(marker, output)
            self.assertNotIn("direct trace", output)

    def test_sink_thresholds_still_apply_after_module_filter(self):
        console, file_output = self.outputs("sink-floor")
        for marker in INFO_MARKERS:
            self.assertIn(marker, console)
        for marker in DEBUG_MARKERS:
            self.assertNotIn(marker, console)
        for marker in ("direct warning", "direct error", "direct critical"):
            self.assertIn(marker, file_output)
        for marker in DEBUG_MARKERS + ("direct info", "direct notice", "macro info"):
            self.assertNotIn(marker, file_output)


if __name__ == "__main__":
    unittest.main()
