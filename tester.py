#! /usr/bin/env python
import httplib, time, sys, math

targets = list(("10.10.5.100",
		"192.168.2.5"))

ports = list((5555,8080,80,12345))

req = "GET"
pages = list(("/", "/i100.html", "/i1K.html", "/i10K.html", "/i100K.html", "/i1M.html"))

times = []
avg = 0;
sd = 0;

def average(s): 
	return sum(s) * 1.0 / len(s)

def warmup(dst, dport, page):
	conn = httplib.HTTPConnection(targets[dst], ports[dport])
	for i in range(num_req):
		conn.request(req, pages[page])
		ans = conn.getresponse()
		ans.read()
	conn.close()
	
def test(dst, dport, page):
	conn = httplib.HTTPConnection(targets[dst], ports[dport])

	for i in range(num_req):
		init = time.time()
		#headers = {"Cookie": "BFTID=F4D55C094AE5AF33"}	
		headers = {
"Accept": "*/*",
"Host": "10.10.5.100:5555", 
#"User-Agent": "curl/7.22.0 (x86_64-pc-linux-gnu) libcurl/7.22.0 OpenSSL/1.0.1 zlib/1.2.3.4 libidn/1.23 librtmp/2.3",
"Cookie": "teste=F4D55C094AE5AF33"
}
		conn.request(req, pages[page], "", headers)
		#conn.putrequest("GET / HTTP/1.0", "sel", False, False)
		print "VAI RECEBER"
		ans = conn.getresponse()
		print "RECEBEU"
		print ans.read(153)
		end = time.time()
		times.append(end-init)
		
	conn.close()
	
def main(dst, dport, page):
	"""
	print "Warming up..."
	init = time.time()
	warmup(dst, dport, page)
	end = time.time()
	print "Warmup time: "+str(end-init)
	"""
	for i in range(t):
		init = time.time()
		test(dst, dport, page)
		end = time.time()
		print "Test "+str(i)+" time: "+str(end-init)
	"""	
	print "Writing results to files..."
		
	avg = average(times)	
	
	f = open('results.txt', 'a')
	f.write(str(avg*1000)+'\n')
	f.close()
	
	variance = map(lambda x: (x - avg)**2, times)
	sd = math.sqrt(average(variance))

	f = open('sd.txt', 'a')
	f.write(str(sd*1000)+'\n')
	f.close()
	"""
print "Initializing..."
num_req = 1
t = int(sys.argv[4])
main(int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3]))
print "Done!"
