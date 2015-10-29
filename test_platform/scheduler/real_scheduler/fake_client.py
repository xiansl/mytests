#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Hai Tao Yue
#   E-mail  :   bjhtyue@cn.ibm.com
#   Date    :   15/09/28 15:55:15
#   Desc    :
#

import sys

if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit("Usage: "+sys.argv[0]+"<Acc_Name> <Input_Size>")
        sys.exit(1)

    print type(sys.argv[1])
    print type(sys.argv[2])


