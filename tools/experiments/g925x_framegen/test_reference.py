"""CPU checks for prototype's motion direction and rejection. Not a GPU test."""
from random import Random

rng = Random(925)
N = 48
previous = [[rng.random() for x in range(N)] for y in range(N)]
def sample(image, x, y):
    return image[max(0,min(N-1,y))][max(0,min(N-1,x))]
def moved(dx,dy):
    return [[sample(previous,x-dx,y-dy) for x in range(N)] for y in range(N)]
def search(current,x,y):
    def error(dx,dy):
        return sum(abs(sample(current,x+i,y+j)-sample(previous,x+i-dx,y+j-dy))
                   for i in (-2,0,2) for j in (-2,0,2))/9
    best,delta = error(0,0),(0,0)
    def consider(dx,dy):
        nonlocal best,delta
        cost=error(dx,dy)+0.0004*(abs(dx)+abs(dy))
        if cost<best: best,delta=cost,(dx,dy)
    for dy in range(-8,9,4):
        for dx in range(-8,9,4): consider(dx,dy)
    cx,cy=delta
    for dy in range(-2,3):
        for dx in range(-2,3):
            consider(max(-8,min(8,cx+dx)),max(-8,min(8,cy+dy)))
    return delta,best
for dx,dy in ((0,0),(4,0),(-4,4),(8,-8)):
    current=moved(dx,dy)
    delta,cost=search(current,24,24)
    assert delta==(dx,dy), (delta,dx,dy)
    # Midpoint uses opposite half-displacements in previous/current images.
    assert sample(previous,24-dx//2,24-dy//2)==sample(current,24+dx//2,24+dy//2)
    assert cost<0.01
def accept(confidence,cut,in_bounds):
    return not cut and in_bounds and confidence>=0.35
assert accept(0.9,False,True)
assert not accept(0.9,True,True)
assert not accept(0.1,False,True)
assert not accept(0.9,False,False)
print("PASS: mobile frame interpolation CPU reference, motion sign and fallback")
