#!/usr/bin/env python3
"""Export auditable component/net evidence without copying the schematic itself."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('schematic', type=Path)
p.add_argument('--kicad-cli', default='kicad-cli')
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
with tempfile.TemporaryDirectory() as td:
    net = Path(td) / 'board.xml'
    subprocess.run([a.kicad_cli, 'sch', 'export', 'netlist', '--format', 'kicadxml',
                    '-o', str(net), str(a.schematic)], check=True)
    root = ET.parse(net).getroot()
components = {c.get('ref'): c.findtext('value') for c in root.findall('./components/comp')}
nets = []
for net in root.findall('./nets/net'):
    nodes = list(net)
    # Include PHY/MCU nets plus both sides of their series resistors.
    if any(n.get('ref') in ('U206', 'U301', 'U601', 'U602', 'U604', 'R608', 'R609') for n in nodes):
        nets.append({'name': net.get('name'), 'nodes': [dict(n.attrib) for n in nodes]})
source_files = sorted(a.schematic.parent.glob('*.kicad_sch'))
evidence = {'source': a.schematic.name,
            'sha256': {f.name: hashlib.sha256(f.read_bytes()).hexdigest() for f in source_files},
            'components': components, 'nets': nets}
a.output.write_text(json.dumps(evidence, indent=2, ensure_ascii=False) + '\n')
