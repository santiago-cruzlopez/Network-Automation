import socket
import ipaddress  
from bs4 import BeautifulSoup
# Create a socket object
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
# Connect to a remote host
s.connect(('www.example.com', 80))
# Send data
s.send(b'GET / HTTP/1.1\r\nHost: www.example.com\r\n\r\n')
# Receive data
data1 = s.recv(1024)
html_content = data1.decode()
# Parse the HTML
soup = BeautifulSoup(html_content, 'html.parser')
# Extract the HTML code
html_code = soup.prettify()
print(html_code)
# Close the socket
s.close()
#Client-Server Model
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.bind(('localhost', 12345))
server_socket.listen(10)
while True:
    client_socket, address = server_socket.accept()
    print(f"Connection from {address} has been established.")
    client_socket.send(b"Hello, client!")
    client_socket.close()