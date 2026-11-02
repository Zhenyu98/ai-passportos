#!/usr/bin/env python3
"""Static wiring contracts for the badge application's startup key guard."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


def read(relative_path: str) -> str:
    return (ROOT / relative_path).read_text(encoding="utf-8")


def function_body(source: str, name: str) -> str:
    match = re.search(rf"\b{re.escape(name)}\s*\([^;]*?\)\s*\{{", source)
    if not match:
        raise AssertionError(f"function not found: {name}")
    start = match.end() - 1
    depth = 0
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start + 1:index]
    raise AssertionError(f"function is unterminated: {name}")


class KeyWakeGuardWiringTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.main = read("main/passport_app.c")

    def test_boot_guard_is_armed_before_callbacks_can_enqueue(self) -> None:
        body = function_body(self.main, "passport_app_start")
        self.assertLess(body.index("wait_release=bsp_button_read_mv()"),
                        body.index("atomic_store(&input_ready,true)"))
        # A release between sampling and readiness must still disarm the guard:
        # the owner also samples the actual released voltage on every step.
        step=function_body(self.main,"input_step")
        # An individual release is insufficient for a chord: the executable
        # runtime test covers partial release, full release and input loss.
        self.assertIn("bsp_button_read_mv()>=BSP_BTN_MV_RELEASED_MIN",step)


if __name__ == "__main__":
    unittest.main()
