#!/usr/bin/env python

import optparse
import os
import subcommand
import sys

from merge_request import MergeRequest


# git flutter owner: List owners to review the MR.
def CMDowner(parser, args):
  print('owner')
  pass


# git flutterpresubmit: presubmit check.
def CMDpresubmit(parser, args):
  print('presubmit')
  pass


# git flutter build: Run build.
def CMDbuild(parser, args):
  print('CMDbuild')
  pass

# git flutter format: Run clang-format for flutter 
def CMDformat(parser, args):
  print('CMDformat')
  pass

# git fluttercheck: Check the commit log and coding style.
def CMDcheck(parser, args):
  parser.add_option(
      '--all',
      action='store_true',
      help='Format all source files in the project.')
  parser.add_option(
      '--check-style-only',
      action='store_true',
      help='Only check coding style with clang-format.')
  options, args = parser.parse_args(args)
  try:
    from commit_message_helper import CheckCommitMessage
  except ImportError as error:
    print('Import error: %s.' % (error))
    return 1
  old_cwd = os.getcwd()
  mr = MergeRequest()
  os.chdir(mr.GetRootDirectory())
  try:
    # Check commit message
    if not options.check_style_only:
      print('Checking commit message...')
      commit_message = mr.GetCommitLog()
      has_error, message = CheckCommitMessage(commit_message)
      if has_error:
        print('Error checking commit mesage:')
        print('    ==> %s\n' % (message))
        print('The commit message should be formatted as follow:\n')
        print('    {title}(#issueID)\n\n'
              '    {summary}\n\n')
        # NOTE: The return code is also used by CI check.
        sys.exit(1)
      else:
        print('  PASSED.')
  finally:
    os.chdir(old_cwd)

# git flutter lint: Run lint on C/C++ source files.
def CMDlint(parser, args):
  print('CMDlint')
  pass


def CMDhelp(parser, args):
  if not any(i in ('-h', '--help') for i in args):
    args = args + ['--help']
  parser.parse_args(args)
  assert False


class OptionParser(optparse.OptionParser):
  def __init__(self, *args, **kwargs):
    optparse.OptionParser.__init__(
        self, *args, prog='git flutter',  **kwargs)

  def parse_args(self, args=None, _values=None):
    try:
      return self._parse_args(args)
    finally:
      pass

  def _parse_args(self, args=None):
    # Create an optparse.Values object that will store only the actual passed
    # options, without the defaults.
    actual_options = optparse.Values()
    _, args = optparse.OptionParser.parse_args(self, args, actual_options)
    # Create an optparse.Values object with the default options.
    options = optparse.Values(self.get_default_values().__dict__)
    # Update it with the options passed by the user.
    options._update_careful(actual_options.__dict__)
    return options, args


def main(argv):
  usage = 'git flutter subcommand'
  dispatcher = subcommand.CommandDispatcher(__name__)

  dispatcher.execute(OptionParser(), argv)

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
