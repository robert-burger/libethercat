#!/usr/bin/env python3
import os

with open('content.html', 'w') as f:
    f.write("""<!DOCTYPE html>""")
    for element in sorted(os.listdir(os.getcwd())):
        if os.path.isdir(element) and not element.startswith("."):
            emph_element = element
            f.write('<li><a class="index.html" href="' + element + '/index.html"><div class="item"><span class="label">' + emph_element + '</span></div></a></li>')
