from flask import Flask, jsonify, render_template, request, json
import pymysql
from datetime import datetime


import atexit

from apscheduler.schedulers.background import BackgroundScheduler
from apscheduler.triggers.interval import IntervalTrigger

from temperature import temperature

'''
scheduler = BackgroundScheduler()
scheduler.start()
scheduler.add_job(
    func=print_date_time,
    trigger=IntervalTrigger(seconds=5),
    id='printing_job',
    name='Print date and time every five seconds',
    replace_existing=True)
# Shut down the scheduler when exiting the app
atexit.register(lambda: scheduler.shutdown())
'''

from ctypes import *


app = Flask(__name__)

conn = pymysql.connect(host='localhost', user='jong', password='jong',
                               db='iotdb', charset='utf8')

curs = conn.cursor()


Temperature = temperature.Temperature()

fan = CDLL('./fan.so')
gas_ = CDLL('./gas.so')

'''
from temperature import temperature
from flask_restful import Resource, Api

api = Api(app)
api.add_resource(temperature.Temperature, '/hello')
'''

from util.util import myJsonify

@app.route("/")
def main():
    return render_template('webapp/main.html')


@app.route("/temperature")
def temperature():
    sql = "SELECT device_id, current_temperature, want_temperature, button, auto_mode FROM temperature;"
    curs.execute(sql)
    rows = curs.fetchall()
    temp_list = [r for r in rows[0]]
    temp_list[2] = int(temp_list[2])

    key_list = ["device_id", "current_temperature", "want_temperature", "button", "auto_mode"]
    temp_dict = myJsonify(key_list, temp_list)

    return render_template('webapp/temperature.html', temp_dict=temp_dict)

def insert_temp_log(device_id, user_name):
    # 온도 테이블 가져오기
    data = [device_id]
    sql = Temperature.select_temperature()
    curs.execute(sql, data)
    rows = curs.fetchall()
    temp_table_list = [r for r in rows[0]]

     #로그 온도 테이블 기록
    temp_table_list.insert(1, user_name)
    sql = Temperature.insert_log()
    curs.execute(sql, temp_table_list)
    conn.commit()


@app.route("/temperature/button", methods=['POST'])
def temperature_button():
    json_dict = request.get_json()
    device_id = json_dict['device_id']
    is_checked = json_dict['is_checked']
    if(is_checked == True):
        status = 'ON'
        status_code = 1
    else:
        status = 'OFF'
        status_code = 0

    print('start')

    charptr = POINTER(c_char)

    #fan.controlFan.restype = charptr
    fan.controlFan.argtype = charptr

    buf = create_string_buffer(256)

    result = fan.controlFan(buf, status_code)
    print('call')
    if(result < 0):
        print("error")
        return json.dumps({'return_code': -1, 'return_message': 'fail'})

    result_data = cast(buf, c_char_p).value
    result_data = result_data.decode('utf-8')
    result_split = result_data.split(',')
    cur_temp = result_split[0]
    print(cur_temp)

    user_name = json_dict['user_name']

    # 온도 테이블 업데이트
    data = [status, cur_temp, device_id]
    sql = Temperature.update_temperature_button()
    curs.execute(sql, data)
    conn.commit()

    insert_temp_log(device_id, user_name)

    result = {'return_code': 0, 'return_message': 'success'}
    return json.dumps(result)


@app.route("/temperature/set_temp", methods=['POST'])
def temperature_set_temp():
    json_dict = request.get_json()
    device_id = json_dict['device_id']
    want_temp = json_dict['want_temp']
    user_name = json_dict['user_name']

    # 온도 테이블 업데이트
    data = [want_temp, device_id]
    sql = Temperature.update_temperature_want()
    curs.execute(sql, data)
    conn.commit()

    insert_temp_log(device_id, user_name)

    result = {'return_code': 0, 'return_message': 'success'}
    return json.dumps(result)


@app.route("/temperature/auto_mode", methods=['POST'])
def temperature_auto_mode():
    json_dict = request.get_json()
    device_id = json_dict['device_id']
    is_checked = json_dict['is_checked']
    if (is_checked == True):
        status = 'ON'
    else:
        status = 'OFF'

    user_name = json_dict['user_name']

    # 온도 테이블 업데이트
    data = [status, device_id]
    sql = Temperature.update_temperature_auto()
    curs.execute(sql, data)
    conn.commit()

    insert_temp_log(device_id, user_name)

    result = {'return_code': 0, 'return_message': 'success'}
    return json.dumps(result)


@app.route("/gas")
def gas():
    return render_template('webapp/gas.html')

@app.route("/gas/button")
def gas_button():
    is_checked = request.args.get('isChecked', '')
    if (is_checked == "on"):
        button_onoff = 'ON'
        result = gas_.gas_open()
    else:
        button_onoff = 'OFF'
        result = gas_.gas_close()

    print(result)
    '''
    user_name = request.args.get('userName', '')
    status = request.args.get('status', '')
    #auto_mode = request.args.get('auto_mode', '')
    auto_mode = 'OFF'
    data = [user_name, status, button_onoff, auto_mode]

    sql = Gas.insert_log()
    curs.execute(sql, data)
    conn.commit()
    '''

    result = {'return_code': 0, 'return_message': 'success'}
    return json.dumps(result)


@app.route("/lights")
def lights():
    return render_template('webapp/lights.html')

@app.route("/lights/button")
def lights_button():
    is_checked = request.args.get('isChecked', '')
    if (is_checked == "true"):
        button_onoff = 'ON'
    else:
        button_onoff = 'OFF'

    user_name = request.args.get('userName', '')
    status = request.args.get('status', '')
    # auto_mode = request.args.get('auto_mode', '')
    auto_mode = 'OFF'
    data = [user_name, status, button_onoff, auto_mode]

    sql = Gas.insert_log()
    curs.execute(sql, data)
    conn.commit()

    result = {'return_code': 0, 'return_message': 'success'}
    return json.dumps(result)



@app.route("/dust")
def dust():
    return render_template('webapp/dust.html')


@app.route("/door")
def door():
    return render_template('webapp/door.html')


@app.route("/energy")
def energy():
    return render_template('webapp/energy.html')

@app.route("/energy/product")
def energy_product():
    return render_template('webapp/product.html')

@app.route("/energy/total")
def energy_total():
    return render_template('webapp/totalen.html')


####### admin
@app.route("/admin")
def admin_main():
    return render_template('admin/main.html')


@app.route("/admin/temperature")
def admin_temperature():
    sql = "select user_name, current_datetime, button, current_temperature, want_temperature, auto_mode from log_temperature order by id DESC limit 20"
    curs.execute(sql)

    rows = curs.fetchall()
    table_temp_list = rows
    # 실제 실외온도 가져오기
    graph_temp_list = [24, 23, 24, 25, 26, 27, 26, 27]

    return render_template('admin/temperature.html', table_temp_list=table_temp_list, graph_temp_list=graph_temp_list)


@app.route("/admin/gas")
def admin_gas():
    sql = "select user_name, current_datetime, status, button, auto_mode from log_gas order by id DESC limit 20"
    curs.execute(sql)

    rows = curs.fetchall()
    table_gas_list = rows

    return render_template('admin/gas.html', table_gas_list=table_gas_list)


@app.route("/admin/lights")
def admin_lights():
    sql = "select user_name, room_name, current_datetime, button from log_lights order by id DESC limit 20"
    curs.execute(sql)

    rows = curs.fetchall()
    table_lights_list = rows
    return render_template('admin/lights.html', table_lights_list=table_lights_list)


@app.route("/admin/dust")
def admin_dust():
    sql = "select user_name, current_datetime, dust_amount, status, button, auto_mode from log_dust order by id DESC limit 20"
    curs.execute(sql)

    rows = curs.fetchall()
    table_dust_list = rows
    return render_template('admin/dust.html', table_dust_list=table_dust_list)


@app.route("/admin/doorlock")
def admin_doorlock():
    sql = "select user_name, current_datetime, status, button, auto_mode from log_doorlock order by id DESC limit 20"
    curs.execute(sql)

    rows = curs.fetchall()
    table_doorlock_list = rows

    return render_template('admin/doorlock.html', table_doorlock_list=table_doorlock_list)


@app.route("/admin/power")
def admin_power():
    cur_now = datetime.now()
    cur_year = cur_now.year
    #cur_month = cur_now.month
    cur_month = 10

    sql = "select power from log_power_daily where product_name = '%s' and year = %d and month = %d ORDER BY day ASC;" % ('total', cur_year, cur_month)
    curs.execute(sql)
    rows = curs.fetchall()
    daily_list = [li[0] for li in rows]

    sql = "select power from log_power_monthly where product_name = '%s' and year = %d ORDER BY month ASC;" % ('total', cur_year)
    curs.execute(sql)
    rows = curs.fetchall()
    monthly_list = [li[0] for li in rows]

    return render_template('admin/power.html', daily_list=daily_list, monthly_list=monthly_list)




@app.route("/test")
def test():
    sql = "select * from log_temperature"
    curs.execute(sql)

    rows = curs.fetchall()
    test_data = rows
    time_list = []
    temperature_list = []
    for td in test_data:
        temperature_list.append(td[2])
        time_list.append(str(td[4].hour)+'시')

    return render_template('admin/test.html', time_list=time_list, temperature_list=temperature_list)




#dt = datetime.strptime("25/08/17 11:10", "%d/%m/%y %H:%M")
#dt_str = dt.strftime("%y-%m-%d %H:%M")


if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=8080)
    #app.run(debug=False)
