FROM python:3.12-alpine
# Minimal NAT proxy container for STUN traffic simulation.
# NAT_TYPE env var controls behavior: full-cone, port-restricted, symmetric, symmetric-seq
COPY nat_proxy.py /nat_proxy.py
ENTRYPOINT ["python3", "-u", "/nat_proxy.py"]
