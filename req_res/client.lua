package.path = package.path .. ";/root/dynbandwidth/req_res/json.lua"
json = require "json"

body = ""
for i = 1, 500 do
  body = body .. 'a'
end
req = wrk.format("GET", "/", nil, body) 

request = function ()
  return req
end

done = function(summary, latency, requests)
  local final = {}
  final["summary"] = summary
  final["latency"] = {}
  final["latency"]["avg"] = latency.mean
  final["latency"]["stddev"] = latency.stdev
  for i=0, 10000, 1 do 
    final["latency"][tostring(i)] = latency:percentile(i/100)
  end
  final["latency"]["10000"] = latency.max
  jreport = json.encode(final)
  file = io.open("temp", "w")
  io.output(file)
  io.write(jreport)
  io.close()
end
