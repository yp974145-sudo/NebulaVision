"""协议与失败分支回归；使用HTTP替身，不调用收费接口。"""
import sys
sys.dont_write_bytecode = True
import json
import os
import unittest
import urllib.error
from unittest.mock import patch
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "agent"))
import deepseek

class Reply:
    def __init__(self, content): self.content = content
    def __enter__(self): return self
    def __exit__(self, *args): pass
    def read(self, size): return self.content[:size]

class DeepSeekCheck(unittest.TestCase):
    @patch.object(deepseek, "load_settings", return_value=("fixture-key", "deepseek-flash"))
    def test_request_and_evidence(self, settings):
        def respond(request, timeout):
            self.assertEqual(request.full_url, "https://api.deepseek.com/chat/completions")
            self.assertEqual(request.get_header("Authorization"), "Bearer fixture-key")
            body = json.loads(request.data)
            self.assertNotIn("fixture-key", json.dumps(body))
            self.assertEqual(body["thinking"]["type"], "disabled")
            self.assertFalse(body["stream"])
            self.assertEqual(timeout, 40)
            return Reply(json.dumps({"choices": [{"message": {"content": "依据证据，共17条。"}, "finish_reason": "stop"}]}).encode())
        with patch("urllib.request.build_opener") as opener:
            opener.return_value.open.side_effect = respond
            result = deepseek.generate(Path("."), "今天事件", "事件总数：17")
        self.assertIn("共17条", result)
        self.assertTrue(result.endswith("事件总数：17"))

    def test_no_key(self):
        with patch.dict(os.environ, {}, clear=True), patch.object(Path, "exists", return_value=False):
            with self.assertRaisesRegex(deepseek.ModelError, "尚未设置"):
                deepseek.load_settings(Path("."))

    @patch.object(deepseek, "load_settings", return_value=("fixture-key", "deepseek-flash"))
    def test_errors_do_not_leak(self, settings):
        for code in (401, 402, 429, 500, 302):
            with self.subTest(code=code), patch("urllib.request.build_opener") as opener:
                opener.return_value.open.side_effect = urllib.error.HTTPError(
                    "https://api.deepseek.com", code, "fixture-key secret body", {}, None)
                with self.assertRaises(deepseek.ModelError) as failure:
                    deepseek.generate(Path("."), "问题", "证据")
                self.assertNotIn("fixture-key", str(failure.exception))
                self.assertEqual(opener.return_value.open.call_count, 1)

    @patch.object(deepseek, "load_settings", return_value=("fixture-key", "deepseek-flash"))
    def test_timeout_and_malformed(self, settings):
        with patch("urllib.request.build_opener") as opener:
            for value in (b"not json", b'{}', b'{"choices":[{"finish_reason":"length","message":{"content":"partial"}}]}'):
                opener.return_value.open.return_value = Reply(value)
                with self.assertRaises(deepseek.ModelError):
                    deepseek.generate(Path("."), "问题", "证据")
            opener.return_value.open.side_effect = TimeoutError("fixture-key")
            with self.assertRaises(deepseek.ModelError):
                deepseek.generate(Path("."), "问题", "证据")

if __name__ == "__main__": unittest.main()
