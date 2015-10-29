#!/usr/bin/env python
# -*- coding:utf-8 -*-
#
#   Author  :   Zhuang Di ZHU
#   E-mail  :   zdzhubj@cn.ibm.com
#   Date    :   15/07/05 13:36:19
#   Desc    :
#
import ConfigParser


def read_db_config(filename='config.ini', section='mysql'):
    """Read databse configuration file and return a dictionary object
    :param filename: name of the configuration file
    :param section: section of database configuration
    :return: a dictionary of database parameters
    """

    # create parser and read ini configuration file
    parser = ConfigParser.ConfigParser()
    parser.read(filename)

    # get section, default to mysql
    db = {}
    if parser.has_section(section):
        items = parser.items(section)
        for item in items:
            db[item[0]] = item[1]

    else:
        raise Exception('{0} not found in the {1} file'. format(section, filename))

    return db
