"""System tests: console login, console log content, security-event log (TC31, C-30).

Owner rules: no password in the repo (the console password comes from the environment variable
SH_CONSOLE_PASSWORD, user `console`); a test never sets, changes or removes the password, never runs a
command that changes a setting or an output, and at most ONE wrong password is sent per run
(loginAttempts = 5 per client address). The lockout itself (FR-75.4) is not tested.
"""
from .base import *  # noqa: F401,F403

CONSOLE_USER = "console"
PASSWORD_ENV = "SH_CONSOLE_PASSWORD"
WRONG_PASSWORD = "wrong-password-for-test"

# FR-76: assignments / values that must never appear in the console log
SECRET_PATTERNS = [r"password\s*[=:]", r"passwd", r"pass:", r"12345678", r"admin123", r"secret",
                   r"token", r"apikey", r"api_key", r"sha256"]
# FR-76: a pair of decimal coordinates (house location)
COORD_PATTERN = r"\d{2}\.\d{3,}, ?\d{2}\.\d{3,}"

EVENT_TYPES = {"login-fail", "login-ok", "lockout", "command", "password-change", "factory-reset"}
EVENT_SOURCES = {"http", "telnet", "serial"}
CLIENT_PATTERN = r"^\d{1,3}\.\d{1,3}\.\d{1,3}\.x$"  # FR-77: last part removed

# One wrong password per run: the single attempt is made once and shared by all tests.
_WRONG_LOGIN = {}


class ConsoleTests:

    # ----- helpers -----
    def _con_password(self):
        return os.environ.get(PASSWORD_ENV)

    def _con_skip(self, test_id, name, req, start, reason):
        return TestResult(test_id, name, req, "SKIP", (time.time() - start) * 1000, reason)

    def _con_dev_skip(self, test_id, name, req, start):
        """FR-75.6: the release rules (login required) do not apply on a dev-open build."""
        if self.console_profile() == "dev-open":
            return self._con_skip(test_id, name, req, start, "development profile: console open (FR-75.6)")
        return None

    def _con_events(self):
        r = requests.get(f"{self.base_url}/api/security/events", timeout=self.timeout)
        assert r.status_code == 200, f"/api/security/events HTTP {r.status_code}"
        return r.json()

    def _con_count(self, events, etype, source="http"):
        return sum(1 for e in events if e.get("type") == etype and e.get("source") == source)

    def _con_wrong_login_once(self):
        """Send the one wrong-password attempt of the run (cmd=CLEAR) and cache what was seen."""
        if _WRONG_LOGIN:
            return _WRONG_LOGIN
        logs_before = requests.get(f"{self.base_url}/api/console/logs", timeout=self.timeout).text
        ev_before = self._con_events()
        r = requests.post(f"{self.base_url}/api/console/send?cmd=CLEAR",
                          auth=(CONSOLE_USER, WRONG_PASSWORD), timeout=self.timeout)
        logs_after = requests.get(f"{self.base_url}/api/console/logs", timeout=self.timeout).text
        ev_after = self._con_events()
        _WRONG_LOGIN.update(status=r.status_code, headers=r.headers, logs_before=logs_before,
                            logs_after=logs_after, ev_before=ev_before, ev_after=ev_after)
        return _WRONG_LOGIN

    # ----- (a) FR-75.1 -----
    def test_tc31_1_login_required_except_help_status(self):
        """SWE.5-TC31.1: Console login: HELP/STATUS public, other commands 401 (FR-75.1)"""
        test_id = "SWE.5-TC31.1"
        name = "Console Login Required Except HELP and STATUS"
        req = "FR-75.1"
        start = time.time()
        skip = self._con_dev_skip(test_id, name, req, start)
        if skip:
            return skip
        try:
            for cmd in ("HELP", "STATUS"):
                r = requests.post(f"{self.base_url}/api/console/send?cmd={cmd}", timeout=self.timeout)
                assert r.status_code == 200, f"{cmd} without login must answer 200, got {r.status_code}"

            logs_before = requests.get(f"{self.base_url}/api/console/logs", timeout=self.timeout).text
            r = requests.post(f"{self.base_url}/api/console/send?cmd=CLEAR", timeout=self.timeout)
            assert r.status_code == 401, f"CLEAR without login must answer 401, got {r.status_code}"
            assert "WWW-Authenticate" in r.headers, "401 without WWW-Authenticate header"
            logs_after = requests.get(f"{self.base_url}/api/console/logs", timeout=self.timeout).text
            # "no command runs": a CLEAR would empty the log; a text from the middle of the old log must survive
            if len(logs_before) >= 60:
                mid = logs_before[len(logs_before) // 2: len(logs_before) // 2 + 30]
                assert mid in logs_after, "Log changed after a refused CLEAR: the command seems to have run"
            details = "HELP/STATUS 200 without login; CLEAR 401 + WWW-Authenticate, log kept"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, details)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc31_2_wrong_password_refused(self):
        """SWE.5-TC31.2: Console login: wrong password answers 401, no command runs (FR-75.1)"""
        test_id = "SWE.5-TC31.2"
        name = "Console Wrong Password Refused"
        req = "FR-75.1"
        start = time.time()
        skip = self._con_dev_skip(test_id, name, req, start)
        if skip:
            return skip
        try:
            w = self._con_wrong_login_once()
            assert w["status"] == 401, f"Wrong password must answer 401, got {w['status']}"
            assert "WWW-Authenticate" in w["headers"], "401 without WWW-Authenticate header"
            if len(w["logs_before"]) >= 60:
                mid = w["logs_before"][len(w["logs_before"]) // 2: len(w["logs_before"]) // 2 + 30]
                assert mid in w["logs_after"], "Log changed after a refused CLEAR (wrong password)"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "1 wrong password: 401 + WWW-Authenticate, log kept")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc31_3_login_works(self):
        """SWE.5-TC31.3: Console login with the right password works (FR-75.1, FR-45)"""
        test_id = "SWE.5-TC31.3"
        name = "Console Login Works"
        req = "FR-75.1, FR-45"
        start = time.time()
        skip = self._con_dev_skip(test_id, name, req, start)
        if skip:
            return skip
        pw = self._con_password()
        if not pw:
            return self._con_skip(test_id, name, req, start, f"{PASSWORD_ENV} not set: login test skipped")
        try:
            ev0 = self._con_events()
            r = requests.post(f"{self.base_url}/api/console/send?cmd=HELP",
                              auth=(CONSOLE_USER, pw), timeout=self.timeout)
            note = ""
            if r.status_code == 403:
                # FR-75.2: first password still has to be changed; the login itself was accepted
                assert "password change required" in r.text, f"403 with unexpected body: {r.text[:100]}"
                note = " (board still requires the first password change, 403 accepted)"
            else:
                assert r.status_code == 200, f"HELP with the right password must answer 200, got {r.status_code}"
            assert r.status_code not in (401, 429), f"Right password refused: {r.status_code}"
            ev1 = self._con_events()
            e0, e1 = ev0.get("events", []), ev1.get("events", [])
            if len(e0) < 50:
                # FR-77: successful login is recorded; HELP changes nothing, so no command event
                assert self._con_count(e1, "login-ok") > self._con_count(e0, "login-ok"), \
                    "No login-ok event of source http after a successful login"
                assert self._con_count(e1, "command") == self._con_count(e0, "command"), \
                    "HELP was recorded as a command event (changes nothing)"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"HELP with login: HTTP {r.status_code}{note}")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc31_4_no_login_only_help_status(self):
        """SWE.5-TC31.4: Without a login only HELP and STATUS work (FR-75.2)"""
        test_id = "SWE.5-TC31.4"
        name = "Without Login Only HELP and STATUS"
        req = "FR-75.2"
        start = time.time()
        skip = self._con_dev_skip(test_id, name, req, start)
        if skip:
            return skip
        try:
            for cmd in ("HELP", "STATUS"):
                r = requests.post(f"{self.base_url}/api/console/send?cmd={cmd}", timeout=self.timeout)
                assert r.status_code == 200, f"{cmd} without login: {r.status_code}"
            # harmless unknown command: everything except HELP/STATUS is refused before it is looked at
            r = requests.post(f"{self.base_url}/api/console/send?cmd=NOSUCHCMD", timeout=self.timeout)
            assert r.status_code == 401, f"Other command without login must answer 401, got {r.status_code}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "HELP/STATUS 200, other command 401")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    # ----- (b) FR-75.5 -----
    def test_tc31_5_route_cmd_removed(self):
        """SWE.5-TC31.5: /api/console/cmd does not exist (FR-75.5)"""
        test_id = "SWE.5-TC31.5"
        name = "Route /api/console/cmd Removed"
        req = "FR-75.5"
        start = time.time()
        try:
            r = requests.post(f"{self.base_url}/api/console/cmd?cmd=HELP", timeout=self.timeout)
            assert r.status_code == 404, f"/api/console/cmd must answer 404, got {r.status_code}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, "HTTP 404")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def _con_page_text(self):
        r = requests.get(f"{self.base_url}/", timeout=self.timeout)
        assert r.status_code == 200, f"Page HTTP {r.status_code}"
        text = r.text
        for src in re.findall(r'<script[^>]+src="([^"]+)"', text):
            if src.startswith("http"):
                continue
            rs = requests.get(f"{self.base_url}/{src.lstrip('/')}", timeout=self.timeout)
            if rs.status_code == 200:
                text += "\n" + rs.text
        return text

    def test_tc31_6_page_uses_send_route(self):
        """SWE.5-TC31.6: Page uses /api/console/send, not /api/console/cmd (FR-75.5)"""
        test_id = "SWE.5-TC31.6"
        name = "Page Uses Console Send Route"
        req = "FR-75.5"
        start = time.time()
        try:
            text = self._con_page_text()
            assert "api/console/send" in text, "Page does not use api/console/send"
            assert "api/console/cmd" not in text, "Page still refers to api/console/cmd"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000, "send route only")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc31_7_page_password_not_stored(self):
        """SWE.5-TC31.7: Page keeps the password in memory only, never in page storage (FR-75.5)"""
        test_id = "SWE.5-TC31.7"
        name = "Page Does Not Store the Password"
        req = "FR-75.5"
        start = time.time()
        try:
            text = self._con_page_text()
            for m in re.finditer(r"(localStorage|sessionStorage)\s*(\.\s*setItem|\[)", text):
                window = text[max(0, m.start() - 200): m.end() + 200].lower()
                assert "pass" not in window, \
                    f"Page writes to {m.group(1)} near the word password: ...{window[150:300]}..."
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "no storage write near 'password'")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    # ----- (c) FR-76 -----
    def test_tc31_8_log_has_no_secret(self):
        """SWE.5-TC31.8: Console log holds no password, hash, key or token (FR-76)"""
        test_id = "SWE.5-TC31.8"
        name = "Console Log Without Secrets"
        req = "FR-76"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/console/logs", timeout=self.timeout)
            assert r.status_code == 200, f"logs HTTP {r.status_code}"
            for pat in SECRET_PATTERNS:
                m = re.search(pat, r.text, re.IGNORECASE)
                assert not m, f"Console log matches /{pat}/: ...{r.text[max(0, m.start()-40): m.end()+40]}..."
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"{len(SECRET_PATTERNS)} secret patterns absent")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc31_9_log_has_no_coordinates(self):
        """SWE.5-TC31.9: Console log and STATUS output hold no house coordinates (FR-76)"""
        test_id = "SWE.5-TC31.9"
        name = "Console Log Without House Coordinates"
        req = "FR-76"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/console/logs", timeout=self.timeout)
            assert r.status_code == 200, f"logs HTTP {r.status_code}"
            m = re.search(COORD_PATTERN, r.text)
            assert not m, f"Coordinates in the console log: {m.group(0)}"
            # STATUS is public (FR-75.1); its output goes to the same log
            r2 = requests.post(f"{self.base_url}/api/console/send?cmd=STATUS", timeout=self.timeout)
            assert r2.status_code == 200, f"STATUS HTTP {r2.status_code}"
            m = re.search(COORD_PATTERN, r2.text)
            assert not m, f"Coordinates in the STATUS output: {m.group(0)}"
            for pat in SECRET_PATTERNS:
                m = re.search(pat, r2.text, re.IGNORECASE)
                assert not m, f"STATUS output matches /{pat}/"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "no coordinate pair in log and STATUS")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    # ----- (d) FR-77 -----
    def test_tc31_10_security_events_schema(self):
        """SWE.5-TC31.10: Security-event list schema, at most 50, client a.b.c.x (FR-77)"""
        test_id = "SWE.5-TC31.10"
        name = "Security Events Schema"
        req = "FR-77"
        start = time.time()
        try:
            data = self._con_events()
            ev = data.get("events")
            assert isinstance(ev, list), "events is not a list"
            assert data.get("count") == len(ev), f"count {data.get('count')} != len(events) {len(ev)}"
            assert len(ev) <= 50, f"More than 50 events: {len(ev)}"  # ring of the last 50
            last = None
            for e in ev:
                assert isinstance(e.get("time"), (int, float)), f"time missing: {e}"
                assert e.get("type") in EVENT_TYPES, f"type {e.get('type')}"
                assert e.get("source") in EVENT_SOURCES, f"source {e.get('source')}"
                if e["source"] in ("http", "telnet"):
                    assert re.match(CLIENT_PATTERN, str(e.get("client", ""))), \
                        f"client not a.b.c.x: {e.get('client')}"
                # oldest first (time stamps never go back)
                if last is not None:
                    assert e["time"] >= last, f"Not oldest first: {e['time']} after {last}"
                last = e["time"]
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"{len(ev)} events, schema ok")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc31_11_failed_login_recorded(self):
        """SWE.5-TC31.11: A failed login is recorded as login-fail, source http (FR-77)"""
        test_id = "SWE.5-TC31.11"
        name = "Failed Login Recorded"
        req = "FR-77"
        start = time.time()
        skip = self._con_dev_skip(test_id, name, req, start)
        if skip:
            return skip
        try:
            w = self._con_wrong_login_once()
            before = w["ev_before"].get("events", [])
            after = w["ev_after"].get("events", [])
            if len(before) >= 50:
                return self._con_skip(test_id, name, req, start,
                                      "ring is full (50 events): counts cannot be compared")
            b = self._con_count(before, "login-fail")
            a = self._con_count(after, "login-fail")
            assert a == b + 1, f"login-fail http events {b} -> {a}, expected +1 after one wrong password"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"login-fail http {b} -> {a}")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    # ----- (f) FR-75.2 / FR-77 never recorded -----
    def test_tc31_12_events_hold_no_secret_field(self):
        """SWE.5-TC31.12: Security events never carry a password or hash field (FR-77, FR-75.2)"""
        test_id = "SWE.5-TC31.12"
        name = "Security Events Without Secret Fields"
        req = "FR-77, FR-75.2"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/security/events", timeout=self.timeout)
            assert r.status_code == 200, f"HTTP {r.status_code}"
            for e in r.json().get("events", []):
                extra = set(e.keys()) - {"time", "type", "source", "client"}
                assert not extra, f"Unexpected event fields: {extra}"
            for word in ("password", "passwd", "hash", WRONG_PASSWORD):
                assert word not in r.text.lower(), f"'{word}' found in the security events"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "only time/type/source/client")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    # ----- FR-75.6 development profile -----
    def test_tc31_13_profile_and_open_console(self):
        """SWE.5-TC31.13: /api/status profile, banner and open console follow the profile (FR-75.6)"""
        test_id = "SWE.5-TC31.13"
        name = "Profile, Banner and Console Access"
        req = "FR-75.6"
        start = time.time()
        try:
            r = requests.get(f"{self.base_url}/api/status", timeout=self.timeout)
            assert r.status_code == 200, f"/api/status HTTP {r.status_code}"
            profile = r.json().get("profile")
            assert profile in ("dev-open", "release"), f"profile must be dev-open or release, got {profile}"
            banner = "DEV - consola deschisă"
            page = self._con_page_text()
            # harmless log line, not a security event
            rs = requests.post(f"{self.base_url}/api/console/send?cmd=LOG dev-open-check", timeout=self.timeout)
            if profile == "dev-open":
                assert banner in page, "dev-open build: page lacks the banner"
                assert rs.status_code == 200, f"dev-open: LOG without login must answer 200, got {rs.status_code}"
            else:
                assert banner not in page, "release build: page shows the development banner"
                assert rs.status_code == 401, f"release: LOG without login must answer 401, got {rs.status_code}"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              f"profile {profile}: banner and LOG answer as required")
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))

    def test_tc31_14_dev_open_command_event(self):
        """SWE.5-TC31.14: Dev-open: command without login runs and is recorded as an event (FR-75.6, FR-77)"""
        test_id = "SWE.5-TC31.14"
        name = "Dev-open Command Recorded"
        req = "FR-75.6, FR-77"
        start = time.time()
        try:
            if self.console_profile() != "dev-open":
                return self._con_skip(test_id, name, req, start, "release profile: console not open (FR-75.6)")
            before = self._con_events().get("events", [])
            # TIME: read-only, but not HELP/STATUS/LOG, so it would need a login on a release build
            r = requests.post(f"{self.base_url}/api/console/send?cmd=TIME", timeout=self.timeout)
            assert r.status_code == 200, f"dev-open: TIME without login must answer 200, got {r.status_code}"
            after = self._con_events().get("events", [])
            note = ""
            if len(before) >= 50:
                note = " (ring full: event count not compared)"
            else:
                b = self._con_count(before, "command", source="http")
                a = self._con_count(after, "command", source="http")
                assert a == b + 1, f"command events {b} -> {a}, expected +1"
            return TestResult(test_id, name, req, "PASS", (time.time() - start) * 1000,
                              "TIME 200 without login" + note)
        except Exception as e:
            return TestResult(test_id, name, req, "FAIL", (time.time() - start) * 1000, str(e))
