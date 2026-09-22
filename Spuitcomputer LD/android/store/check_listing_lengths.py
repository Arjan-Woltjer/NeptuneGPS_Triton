# -*- coding: utf-8 -*-
"""
Check the store copy against Play's field limits (NeptuneGPS_Triton#130).

Play silently truncates nothing -- it refuses the field -- and finding that
out while pasting into the Console is a poor use of a Console session. Run
this after editing store/play-listing.md:

    python store/check_listing_lengths.py
"""
import io
import os
import re
import sys

LIMITS = {'Title': 30, 'Short description': 80, 'Full description': 4000}
BANNED = ['Loofdoes']          # trademark; must not appear in the listing

here = os.path.dirname(os.path.abspath(__file__))
text = io.open(os.path.join(here, 'play-listing.md'), encoding='utf-8').read()

# Each field is a "### <name> (limit N)" heading followed by a fenced block.
pattern = re.compile(r'^### (?P<field>[^(\n]+?)\s*\(limit (?P<limit>\d+)\)\s*\n+```\n(?P<body>.*?)\n```',
                     re.S | re.M)

failures = []
checked = 0
for m in pattern.finditer(text):
    field = m.group('field').strip()
    declared = int(m.group('limit'))
    body = m.group('body').strip()
    checked += 1

    expected = LIMITS.get(field)
    if expected is not None and expected != declared:
        failures.append('%s: heading says limit %d, Play\'s limit is %d' % (field, declared, expected))

    if len(body) > declared:
        failures.append('%s: %d characters, limit %d' % (field, len(body), declared))
    else:
        print('OK   %-20s %4d / %d' % (field, len(body), declared))

    for word in BANNED:
        if word.lower() in body.lower():
            failures.append('%s: contains banned word %r' % (field, word))

if checked == 0:
    failures.append('no fields found -- has play-listing.md changed shape?')

for word in BANNED:
    if word.lower() in text.lower() and ('"%s"' % word) not in text:
        failures.append('%r appears in play-listing.md outside the rule that forbids it' % word)

if failures:
    print('\nFAILED:')
    for f in failures:
        print('  - %s' % f)
    sys.exit(1)

print('\nall %d fields within limits' % checked)
