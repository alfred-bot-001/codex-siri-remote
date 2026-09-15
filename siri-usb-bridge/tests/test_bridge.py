import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('bridge',Path(__file__).parents[1]/'scripts/bridge.py')
b=importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)

class BridgeChecks(unittest.TestCase):
    def test_option_is_left_modifier_and_release_is_empty(self):
        self.assertEqual(b.key_report(0x20),bytes([4,0,0,0,0,0,0,0]))
        self.assertEqual(b.key_report(0),bytes(8))

    def test_lock_screen_report(self):
        self.assertEqual(b.key_report(0x10),bytes([9,0,0x14,0,0,0,0,0]))

    def test_return_and_option_can_coexist(self):
        self.assertEqual(b.key_report(0x21),bytes([4,0,0x28,0,0,0,0,0]))

    def test_audio_and_touch_are_never_keyboard_reports(self):
        event=b.parse_event('2026-09-14T00:00:00Z input report_id=0xFA raw=00 00 01 00 03 f8 ff fe')
        self.assertEqual(event[0],0xfa)
        self.assertIsNone(b.parse_event('Connection failed: test'))
        self.assertEqual(b.parse_event('input report_id=0xFB raw=20 00 | buttons=Siri'),(0xfb,b'\x20\x00'))

    def test_audio_packet_bounds(self):
        for payload in (b'',bytes(99),bytes([0,0,0,0,95,1]),bytes([0,0,0,0,3,1])):
            self.assertIsNone(b.opus_packet(payload))
        self.assertEqual(b.opus_packet(bytes([0,0,255,255,3,0xf8,0xff,0xfe])),(65535,b'\xf8\xff\xfe'))

    def test_usb_disconnect_discards_queued_presses(self):
        with tempfile.NamedTemporaryFile() as f:
            kb=b.Keyboard(f.name)
            kb.send(b.key_report(0x10))
            kb.clear()
            kb.flush()
            self.assertEqual(Path(f.name).read_bytes(),bytes(8))
            kb.close()

    def test_duplicate_reports_do_not_accumulate(self):
        with tempfile.NamedTemporaryFile() as f:
            kb=b.Keyboard(f.name)
            for _ in range(100): kb.send(b.key_report(0x20))
            self.assertEqual(len(kb.pending),2)
            kb.send(bytes(8)); kb.flush()
            self.assertEqual(Path(f.name).read_bytes(),bytes(8)+b.key_report(0x20)+bytes(8))
            kb.close()

    @unittest.skipUnless(sys.platform=='linux','Opus integration runs on the Pi')
    def test_real_opus_silence_packet(self):
        decoder=b.Opus()
        pcm=decoder.decode(b'\xf8\xff\xfe')
        self.assertEqual(len(pcm),1920)
        self.assertEqual(pcm,bytes(1920))
        decoder.reset()

if __name__=='__main__': unittest.main()
