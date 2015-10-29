#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Hai Tao Yue
#   E-mail  :   bjhtyue@cn.ibm.com
#   Date    :   15/08/14 09:46:24
#   Desc    :
#
def generate_poisson_random(lamda):
    """
    poisson distribution
    return a integer random number, L is the mean value
    """
    p = 1.0
    k = -1
    e = math.exp(-lamda)
    while p >= e:
        u = random.random()  #generate a random floating point number in the range [0.0, 1.0)
        p *= u
        k += 1
    return k

def generate_
