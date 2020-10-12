#!/usr/bin/env python
# Copyright 2013 The Flutter Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import subprocess
import sys
import os


def MakeStamp(stamp_path):
  dir_name = os.path.dirname(stamp_path)

  if not os.path.isdir(dir_name):
    os.makedirs()

  with open(stamp_path, 'a'):
    os.utime(stamp_path, None)


def main():
  parser = argparse.ArgumentParser(
      description='Changes the install name of a dylib')

  parser.add_argument('--dylib', type=str)
  parser.add_argument('--install_name', type=str)
  parser.add_argument('--stamp', type=str)
  # BD ADD
  parser.add_argument('--old_install_name', type=str)

  args = parser.parse_args()

  # BD MOD: START
  # subprocess.check_call([
  #   '/usr/bin/env',
  #   'xcrun',
  #   'install_name_tool',
  #   '-id',
  #   args.install_name,
  #   args.dylib,
  # ])
  if args.old_install_name is None:
    subprocess.check_call([
      '/usr/bin/env',
      'xcrun',
      'install_name_tool',
      '-id',
      args.install_name,
      args.dylib,
    ])
  else:
    subprocess.check_call([
      '/usr/bin/env',
      'install_name_tool',
      '-change',
      args.old_install_name,
      args.install_name,
      args.dylib,
    ])
  # END

  MakeStamp(args.stamp)

if __name__ == '__main__':
  sys.exit(main())
