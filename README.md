# smart-iot-home 
 
사용자가 스마트폰, PC에서 가전제품을 제어할 수 있는 Smart IoT 시스템 구축.

## Front-end
#### html5, css, javascript를 이용하여 구현하였고 서버에서 받아오는 데이터를 출력하는 부분에 jinja2 템플릿을 이용하였다. 
#### 온도 파트에서 그래프를 보여주기 위해 chart.js 라는 오픈소스를 사용하였다. 

- webapp (사용자 웹페이지)
>- 사용자가 IoT 시스템을 제어할 수 있는 웹 페이지.
>- **temperature** : 현재 온도 및 희망 온도를 보여주고 팬을 동작시킬 수 있다.
>- **gas** : 가스 탐지 상태 및 가스 밸브를 on/off 제어할 수 있다.
>- **door** : 도어락 open/close 제어할 수 있다.
>- **power** : 각 가전제품들의 전력 사용량을 보여준다. (실제로 연동하기는 어려워 가상 데이터)

- admin (DB 관리 페이지)
>- 사용자가 IoT 기기를 제어했던 로그 기록을 화면에 보여준다. 
>- **temperature** : 유저이름, 시간, 현재 온도, 희망 온도, 팬 상태(on/off) 등의 로그를 보여준다.
>- **gas** : 유저이름, 시간, 가스 탐지 상태(가스 yes/no), 가스 밸브 상태(on/off) 등의 로그를 보여준다.
>- **door** : 유저이름, 시간, 도어락 상태(open/close)
>- **power_manager** : 일/월별 전체 전력 소모량을 통계화하여 그래프로 보여준다.  (실제로 연동하기는 어려워 가상 데이터)



## Back-end
#### 웹 서버로 구현하였고 python flask 프레임워크에서 개발하였다. 데이터베이스는 mysql를 이용하였다. 
#### 라즈베리파이에서 서버를 구동하였고 IoT 센서가 부착된 아두이노 보드와 통신하여 데이터를 가져오고 센서를 제어하였다. 

- 각 기능별 설명
>- **temperature** : zigbee 통신으로 

>- **gas** : 

>- **door** : 

>- **power** :

