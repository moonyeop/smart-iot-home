from ctypes import *

fan = CDLL('./fan.so')

result = fan.main()
print("result = " + str(result))

