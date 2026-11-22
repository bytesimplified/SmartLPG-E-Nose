import unittest

from tools.serial_logger import FIELDS, parse_wire_line


class SerialLogParsingTests(unittest.TestCase):
    def test_normal_reading(self):
        line = ("CSV,1000,850,720,25.5,50.2,1,0,0,"
                "MONITORING (PROTOTYPE ONLY),cooking\n")
        row = parse_wire_line(line)
        self.assertIsNotNone(row)
        self.assertEqual(len(row), len(FIELDS))
        self.assertEqual(row[-1], "cooking")

    def test_missing_environment(self):
        row = parse_wire_line("CSV,2000,860,730,,,0,0,0,WARMING UP - NOT READY,unlabeled")
        self.assertEqual(row[3:5], ["", ""])

    def test_reject_diagnostics_and_headers(self):
        self.assertIsNone(parse_wire_line("MQ6=800 mV"))
        self.assertIsNone(parse_wire_line("CSV,uptime_ms,mq6_mv"))
        self.assertIsNone(parse_wire_line("CSV,100,100,200,23,50,1,0,0,status,unknown"))
        self.assertIsNone(parse_wire_line("CSV,100,100,200,23,50,1,0,0"))
        self.assertIsNone(parse_wire_line("CSV,100,100,200,23,50,1,2,0,status,cooking"))


if __name__ == "__main__":
    unittest.main()
