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
  final["latency"]["min"] = latency.min
  final["latency"]["max"] = latency.max
  final["latency"]["avg"] = latency.mean
  final["latency"]["stddev"] = latency.stdev
  final["latency"]["q1"] = latency:percentile(25)
  final["latency"]["q3"] = latency:percentile(75)
  jreport = json.encode(final)
  file = io.open("temp", "w")
  io.output(file)
  io.write(jreport)
  io.close()
end
