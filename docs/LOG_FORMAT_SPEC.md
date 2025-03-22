# Log Format Specification

## JSON Format
```json
{
    "timestamp": "2024-01-20T15:04:05.123456Z",
    "direction": "X->Y",
    "size": 1234,
    "connection_id": "conn-123",
    "protocol": "TCP",
    "metadata": {
        "source_ip": "192.168.1.1",
        "dest_ip": "192.168.1.2",
        "source_port": 12345,
        "dest_port": 80
    },
    "content": "base64_encoded_content"
}
```

## Binary Format
[Detailed binary format specification]

## Text Format
[Text format specification and examples]