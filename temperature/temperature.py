from flask import Flask, jsonify, render_template, request
import pymysql
from datetime import datetime

import atexit

from apscheduler.schedulers.background import BackgroundScheduler
from apscheduler.triggers.interval import IntervalTrigger

from flask_restful import Resource

class Temperature():
    def insert_log(self):
        sql = '''
            INSERT INTO `iotdb`.`log_temperature`
            (`device_id`,
            `user_name`,
            `current_temperature`,
            `want_temperature`,
            `auto_mode`,
            `button`)
            VALUES( %s, %s, %s, %s, %s, %s)
            '''
        return sql

    def select_temperature(self):
        sql = '''
            SELECT `device_id`, `current_temperature`, `want_temperature`, `auto_mode`, `button`
            FROM `iotdb`.`temperature`            
            WHERE device_id = %s
            '''
        return sql

    def update_temperature_button(self):
        sql = '''
            UPDATE `iotdb`.`temperature` 
            SET `button` = %s, 
                `current_temperature` = %s
            WHERE device_id = %s
            '''
        return sql

    def update_temperature_want(self):
        sql = '''
            UPDATE `iotdb`.`temperature` 
            SET `want_temperature` = %s
            WHERE device_id = %s
            '''
        return sql

    def update_temperature_auto(self):
        sql = '''
            UPDATE `iotdb`.`temperature` 
            SET `auto_mode` = %s
            WHERE device_id = %s
            '''
        return sql
