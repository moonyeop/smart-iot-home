from flask import Flask, jsonify, render_template, request
import pymysql
from datetime import datetime

import atexit

from apscheduler.schedulers.background import BackgroundScheduler
from apscheduler.triggers.interval import IntervalTrigger

from flask_restful import Resource

class Gas():
    def insert_log(self):
        sql = '''
            INSERT INTO `iotdb`.`log_gas`
            (`user_name`,
            `status`,
            `button`,
            `auto_mode`)
            VALUES( %s, %s, %s, %s)
            '''
        return sql
