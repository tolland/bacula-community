#!/usr/bin/env python3
# Copyright (C) 2000-2022 Kern Sibbald
# License: BSD 2-Clause; see file LICENSE-FOSS

# Script used to convert the md5sum output to the md5 bacula format

import sys
import argparse
import re
#import codecs
import base64

md5_re=re.compile('[0-9A-Fa-f]{32}')

parser=argparse.ArgumentParser(description='Convert md5 in Hexa into base64.')
parser.add_argument('file', metavar='FILENAME', help="input file. If no file use STDIN", nargs='*')
parser.add_argument('--keep-padding', action='store_true', help="keep the '=' at the end if any")
args=parser.parse_args()

def handle_file(input_file, remove_padding):
    nb=0
    for line in input_file:
        line=line.rstrip()
        if md5_re.match(line):
            # don't use the line below that does mime encoding (split at 76c & add a \n)
            # b64=codecs.encode(codecs.decode(line, 'hex'), 'base64').decode()
            b64=base64.b64encode(bytes.fromhex(line)).decode()
            if remove_padding:
                b64=b64.rstrip('=')
            print(b64)
            nb = nb + 1
    return nb

nb=0
remove_padding=not args.keep_padding
if args.file:
    for filename in args.file:
        input_file=open(filename)
        nb += handle_file(input_file, remove_padding)
else:
    # use stdin
    nb += handle_file(sys.stdin, remove_padding)

# Raise an error if we have converted nothing
sys.exit(nb == 0)

