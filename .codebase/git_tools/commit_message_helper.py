#!/usr/bin/env python
# Copyright 2020 The Lynx Authors. All rights reserved.

import re

ERROR_NO_ERROR = 0
ERROR_MALFORMED_MESSAGE = 1
ERROR_MISSING_ISSUE = 2

def CheckCommitMessage(message):
  error_code, error_message = ERROR_NO_ERROR, ''
  commit_lines = message.strip().split('\n')
  # skip revert commit
  
  for line in commit_lines[0:]:
    print('%s' % line)
    if re.match(r'This reverts commit *', line):
      return error_code, error_message
    else:
      error_code, error_message \
          = ERROR_MISSING_ISSUE, 'Missing issue'
      if re.match(r'.*\(\#(\d)+\)\ *$', line):
        error_code, error_message = ERROR_NO_ERROR, ''
        break
  return error_code, error_message


if __name__ == '__main__':
  CheckCommitMessage('Intent to fail')
