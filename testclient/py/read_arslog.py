# 本py文件用于收集每包发送时间，接受时间，包号，并进行成功率计算和传输时延分布
import argparse
import re
import json
import csv

log_file = "../../AR.slog"
record_file = "./pattern.json"
start_time = 0
global_pattern = r'\[(.*?)\]|\|(.*?)\|'
global_map = {}
global_info_map = {}


# type trans
def get_right_type(value : str, type : str):
  if type == 'str':
    return value
  elif type == 'int':
    return int(value)
  elif type == 'float':
    return float(value)

# get microsecond (min start)
def get_time(time_str : str) -> int:
  times = re.findall(r'\d+', time_str)
  time = int(times[3]) * 60 * 60 * 1000000 + int(times[4]) * 60 * 1000000 + int(times[5]) * 1000000 + int(times[6])
  global start_time
  if start_time == 0:
    start_time = time
    time = 0
  else:
    time = time - start_time
  return time

# get scid
def get_scid(scid_str : str) -> str:
  match = re.search(r'scid:(.*)', scid_str)
  scid = ""
  if match:
    scid = match.group(1)
  return scid

# get info
def get_info(info_pattern_map : map, info_str_list : list, time, scid : str) -> map:
  res_map = {}
  res_map['time'] = time
  res_map['scid'] = scid
  for ((field, type), info_str) in zip(info_pattern_map.items(), info_str_list):
    match = re.search(r'' + field + ':(.*)', info_str)
    if match:
      res = match.group(1)
      res_map[field] = get_right_type(res, type)
    else:
      res_map = {}
      break
  return res_map

# get init global res map
def get_init_global_res_map(global_map : map) -> map:
  global_res_map = {}
  for (test_name, func_map) in global_map.items():
    global_func_map = {}
    for (func_name, type_map) in func_map.items():
      global_type_map = {}
      for (type_name, _) in type_map.items():
        global_type_map[type_name] = []
      global_func_map[func_name] = global_type_map
    global_res_map[test_name] = global_func_map
  return global_res_map

def get_global_res_map() -> map:
  # read json file for pattern
  with open(record_file, 'r', encoding='utf-8') as file:
    global_map = json.load(file)
    
  # get init info map
  global_info_map = get_init_global_res_map(global_map)
    
  # read log
  with open(log_file, 'r') as file:
    while True:
      # read line
      line = file.readline()
      if not line:
          break
        
      # use regex get infos
      res_matches = re.findall(global_pattern, line)
      results = []
      for match in res_matches:
        for item in match:
          if item:
            results.append(item)
      
      # read infos
      time = get_time(results[0])
      test_name = results[1]
      test_map = {}
      
      if test_name in global_map:
        test_map = global_map[test_name]
      if not test_map:
        continue
      
      scid = get_scid(results[2])
      if (scid == ""):
        continue
      
      func_name = results[3]
      str_list = results[4:]
      
      if func_name in test_map:
        # must be a list
        func_map = test_map[func_name]
        for type_name, type_map in func_map.items():
          res_map = get_info(type_map, str_list, time, scid)
          if res_map:
            global_info_map[test_name][func_name][type_name].append(res_map)
            break
    return global_info_map

def gen_csv(file_name : str, res_list : list):
  fieldnames = [field for field in res_list[0]]
  with open('../data/' + file_name + '.csv', mode='w', newline='') as file:
    writer = csv.DictWriter(file, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(res_list)
         
def main():
  global_info_map = get_global_res_map()
  for (_, test_map) in global_info_map.items():
    for (_, func_map) in test_map.items():
      for (type_name, res_list) in func_map.items():
        if res_list:
          gen_csv(type_name, res_list)
    
main()