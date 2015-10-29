#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Hai Tao Yue
#   E-mail  :   bjhtyue@cn.ibm.com
#   Date    :   15/09/09 11:59:37
#   Desc    :
#
from __future__ import division
import math

def update_bw():
    n = 5

    A = [([0]*n) for i in range(n)]
    B = [([0]*n) for i in range(n)]
    row = [1 for i in range(n)]
    coloum = [1 for i in range(n)]

    bw = [([0]*n) for i in range(n)]

    A[0][1] = 1
    A[2][1] = 1
    A[3][1] = 1

    A[2][4] = 1
    A[3][4] = 1


    for i in range(n):
        for j in range(n):
            B[i][j] = A[i][j]

    row_id = 0
    row_min_bw = 1.1


    co_id = 0
    co_min_bw = 1.1

    print bw
    while bw[2][4] == 0:
        row_min_bw = 1.1
        co_min_bw = 1.1
        for i in range(n):
            row_flow_num = 0
            for j in range(n):
                row_flow_num += B[i][j]
            if row_flow_num != 0:
                c_bw = row[i]/row_flow_num
                if c_bw < row_min_bw:
                    row_min_bw = c_bw
                    row_id = i

        for j in range(n):
            co_flow_num = 0
            for i in range(n):
                co_flow_num += B[i][j]
            if co_flow_num != 0:
                c_bw = coloum[j]/co_flow_num
                if c_bw < co_min_bw:
                    co_min_bw = c_bw
                    co_id = j

        if row_min_bw < co_min_bw:
            print 'row %r, min_bw =%r' %(row_id, row_min_bw)
            row[row_id] = 0
            for j in range(n):
                if B[row_id][j] != 0:
                    bw[row_id][j] = row_min_bw
                    print "bw[%r][%r] =%r" %(row_id, j, bw[row_id][j])
                    coloum[j] -= row_min_bw * B[row_id][j]
                    B[row_id][j] = 0

        else:
            print 'coloum %r, min_bw =%r' %(co_id, co_min_bw)
            coloum[co_id] = 0
            for i in range(n):
                if B[i][co_id] != 0:
                    bw[i][co_id] = co_min_bw
                    print "bw[%r][%r] =%r" %(i, co_id, bw[i][co_id])
                    row[i] -= co_min_bw * B[i][co_id]
                    B[i][co_id] = 0
    print bw

update_bw()









