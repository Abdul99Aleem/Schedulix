#!/usr/bin/env python3
"""Look up pages in the official QNX documentation set on qnx.com.

The qnx.com docs are an Oxygen WebHelp publication on a host that intermittently
refuses connections. Page *lookup* does not need the web UI: the publication
ships a static index of every page as htmlFileInfoList.js, holding entries of
the form "<relative-url>@@@<Title>@@@<description>".

This script downloads that index once, caches it in the system temp directory,
and then greps the cache locally, so repeat lookups cost no network at all.
Only the one topic page you actually need is fetched afterwards.

Usage:
    qnx_doc.py find <text> [...]     search page titles and urls
    qnx_doc.py url <relative-url>    expand a relative url to an absolute one
    qnx_doc.py list [--guide ID]     list indexed pages, grouped by guide
    qnx_doc.py refresh               re-download the cached index

Standard library only. Last verified against the QNX Everywhere docs:
294 pages across 6 guides.
"""

from __future__ import annotations

import argparse
import html
import json
import os
import re
import sys
import tempfile
import time
import urllib.error
import urllib.request

BASE = "https://www.qnx.com/developers/docs/qnxeverywhere/"
INDEX_REL = "oxygen-webhelp/app/search/index/htmlFileInfoList.js"
INDEX_URL = BASE + INDEX_REL
CACHE_FILE = os.path.join(tempfile.gettempdir(), "qnx-doc-index", "htmlFileInfoList.js")
USER_AGENT = "Mozilla/5.0 (compatible; opencode-qnx-doc-index/1.0)"
TIMEOUT = 30
RETRIES = 5


def download(url: str, dest: str) -> str:
    """Fetch url to dest, retrying the flaky connection."""
    last: Exception | None = None
    for attempt in range(1, RETRIES + 1):
        try:
            request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
            with urllib.request.urlopen(request, timeout=TIMEOUT) as response:
                data = response.read().decode("utf-8", "replace")
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, "w", encoding="utf-8") as handle:
                handle.write(data)
            return data
        except (urllib.error.URLError, OSError, TimeoutError) as exc:
            last = exc
            print(
                "  attempt %d/%d failed: %s" % (attempt, RETRIES, getattr(exc, "reason", exc)),
                file=sys.stderr,
            )
            if attempt < RETRIES:
                time.sleep(min(2**attempt, 15))
    raise SystemExit(
        "error: could not download %s after %d attempts: %s\n"
        "qnx.com intermittently refuses connections; retry later." % (url, RETRIES, last)
    )


def load(force: bool = False) -> list[dict[str, str]]:
    """Return the cached page index as a list of {url, title, desc, guide}."""
    if force or not os.path.exists(CACHE_FILE):
        print("downloading page index from %s" % INDEX_URL, file=sys.stderr)
        raw = download(INDEX_URL, CACHE_FILE)
    else:
        with open(CACHE_FILE, encoding="utf-8") as handle:
            raw = handle.read()

    body = re.sub(r"^\s*var\s+htmlFileInfoList\s*=\s*", "", raw.strip(), count=1)
    body = re.sub(r";\s*$", "", body)

    pages = []
    for entry in json.loads(body):
        parts = entry.split("@@@")
        url = parts[0]
        pages.append(
            {
                "url": url,
                "title": html.unescape(parts[1]) if len(parts) > 1 else "",
                "desc": html.unescape(parts[2]) if len(parts) > 2 else "",
                "guide": url.split("/topic/")[0] if "/topic/" in url else "(root)",
            }
        )
    return pages


def cmd_find(args: argparse.Namespace) -> int:
    pages = load(args.refresh)
    needles = [text.lower() for text in args.text]
    matches = [
        page
        for page in pages
        if any(n in page["title"].lower() or n in page["url"].lower() for n in needles)
    ]
    if not matches:
        print("No page title or url matches.", file=sys.stderr)
        print(
            "  - This doc set is 294 pages of tutorials and guides; it has no\n"
            "    C/POSIX System Library reference (no pthread, sigevent, IonWeb).\n"
            "  - To search page contents instead, the full-text index lives at:\n"
            "    %s%s" % (BASE, "oxygen-webhelp/app/search/index/index-1.js"),
            file=sys.stderr,
        )
        return 1
    for page in matches[: args.limit]:
        print("%s%s\n    %s" % (BASE, page["url"], page["title"]))
    if len(matches) > args.limit:
        print("\n... %d more (raise --limit)" % (len(matches) - args.limit), file=sys.stderr)
    return 0


def cmd_url(args: argparse.Namespace) -> int:
    path = args.path
    print(path if path.startswith("http") else BASE + path.lstrip("/"))
    return 0


def cmd_list(args: argparse.Namespace) -> int:
    pages = load(False)
    if args.guide:
        pages = [page for page in pages if args.guide in page["guide"]]
    guides: dict[str, list[dict[str, str]]] = {}
    for page in pages:
        guides.setdefault(page["guide"], []).append(page)
    for guide in sorted(guides):
        print("%s (%d pages)" % (guide, len(guides[guide])))
        for page in guides[guide][: args.limit]:
            print("    %-72s %s" % (page["url"], page["title"][:60]))
        if len(guides[guide]) > args.limit:
            print("    ... %d more" % (len(guides[guide]) - args.limit))
        print()
    print("total: %d pages" % len(pages))
    return 0


def cmd_refresh(args: argparse.Namespace) -> int:
    pages = load(True)
    print("cached %d pages at %s" % (len(pages), CACHE_FILE))
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    sub = parser.add_subparsers(dest="command", required=True)

    find = sub.add_parser("find", help="search page titles and urls")
    find.add_argument("text", nargs="+")
    find.add_argument("-n", "--limit", type=int, default=25)
    find.add_argument("--refresh", action="store_true")
    find.set_defaults(func=cmd_find)

    expand = sub.add_parser("url", help="expand a relative doc url")
    expand.add_argument("path")
    expand.set_defaults(func=cmd_url)

    listing = sub.add_parser("list", help="list indexed pages by guide")
    listing.add_argument("--guide", default=None, help="filter by guide id, e.g. com.qnx.doc.ddk")
    listing.add_argument("-n", "--limit", type=int, default=200)
    listing.set_defaults(func=cmd_list)

    refresh = sub.add_parser("refresh", help="re-download the cached index")
    refresh.set_defaults(func=cmd_refresh)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())