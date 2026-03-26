#!/usr/bin/env python3
"""Convert twister JUnit XML report to an HTML fragment for Doxygen embedding."""

import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path


def parse_test_name(full_name: str) -> tuple[str, str]:
    """Extract suite and test name from 'suite.suite.group.test_name'."""
    parts = full_name.rsplit(".", 1)
    return (parts[0] if len(parts) > 1 else "unknown", parts[-1])


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <twister_report.xml> <output.html>")
        sys.exit(1)

    xml_path, out_path = sys.argv[1], sys.argv[2]
    tree = ET.parse(xml_path)
    root = tree.getroot()

    suites: dict[str, list[dict]] = defaultdict(list)
    total = passed = failed = skipped = 0

    for ts in root.iter("testsuite"):
        for tc in ts.findall("testcase"):
            total += 1
            name = tc.get("name", "")
            classname = tc.get("classname", "")
            time_s = tc.get("time", "0")

            failure = tc.find("failure")
            skip = tc.find("skipped")

            if failure is not None:
                status, css = "FAIL", "fail"
                failed += 1
            elif skip is not None:
                status, css = "SKIP", "skip"
                skipped += 1
            else:
                status, css = "PASS", "pass"
                passed += 1

            suite_name, test_name = parse_test_name(name)
            suites[classname or suite_name].append(
                {"name": test_name, "status": status, "css": css, "time": time_s}
            )

    html = [
        "<html><head>",
        "<style>",
        "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', "
        "Roboto, Oxygen, sans-serif; margin: 2em; color: #1a1a2e; }",
        "h1 { font-size: 1.5em; }",
        "h2 { font-size: 1.15em; margin-top: 1.5em; color: #16213e; }",
        ".summary { font-size: 1.1em; margin: 1em 0; }",
        ".summary .count { font-weight: bold; }",
        ".pass-bg { color: #2e7d32; }",
        ".fail-bg { color: #c62828; }",
        "table { border-collapse: collapse; width: 100%; max-width: 700px; "
        "margin-bottom: 1em; }",
        "th, td { text-align: left; padding: 6px 12px; border-bottom: "
        "1px solid #e0e0e0; }",
        "th { background: #f5f5f5; font-weight: 600; }",
        ".pass { color: #2e7d32; font-weight: 600; }",
        ".fail { color: #c62828; font-weight: 600; }",
        ".skip { color: #f57f17; font-weight: 600; }",
        ".time { color: #757575; text-align: right; }",
        "</style>",
        "</head><body>",
        "<h1>Test Results</h1>",
    ]

    if failed == 0:
        summary_class = "pass-bg"
    else:
        summary_class = "fail-bg"

    html.append(
        f'<p class="summary {summary_class}">'
        f'<span class="count">{passed}/{total}</span> tests passed'
    )
    if failed:
        html.append(f', <span class="count">{failed}</span> failed')
    if skipped:
        html.append(f', <span class="count">{skipped}</span> skipped')
    html.append("</p>")

    for suite_name in sorted(suites.keys()):
        cases = suites[suite_name]
        display = suite_name.replace("terrascope.", "")
        html.append(f"<h2>{display}</h2>")
        html.append("<table><tr><th>Test</th><th>Status</th>"
                     '<th class="time">Time</th></tr>')
        for tc in sorted(cases, key=lambda c: c["name"]):
            html.append(
                f'<tr><td>{tc["name"]}</td>'
                f'<td class="{tc["css"]}">{tc["status"]}</td>'
                f'<td class="time">{tc["time"]}s</td></tr>'
            )
        html.append("</table>")

    html.append("</body></html>")

    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_text("\n".join(html))
    print(f"Wrote {total} test results to {out_path}")


if __name__ == "__main__":
    main()
