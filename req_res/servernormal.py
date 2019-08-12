import numpy as np
from sanic import Sanic
from sanic.response import json, text

app = Sanic()


@app.route('/')
async def test(request):
    return text('a'*int(max(min(np.random.normal(100000, 25000), 512000), 512)))

if __name__ == "__main__":
    app.run(host='10.0.1.6', port=8000, workers=8, access_log=False)
