


def myJsonify(key_list, value_list):
    result = {}
    for i in range(len(key_list)):
        k = key_list[i]
        v = value_list[i]
        result[k] = v

    return result
