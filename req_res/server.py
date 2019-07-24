import random
from sanic import Sanic
from sanic.response import json, text

texts=[text('a'*(500*(2**i))) for i in range(11)]
#texts=['a'*1000000]

app = Sanic()

@app.route('/')
async def test(request):
    return random.choice(texts)

if __name__ == "__main__":
    app.run(host='10.0.1.6', port=8000, workers=8)
