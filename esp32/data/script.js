// get all the elements we need to interact with
ipEle = document.getElementById("ip");

statusEle = document.getElementById("status");
progressEle = document.getElementById("progress");
elapsedEle = document.getElementById("elapsed");
positionEle = document.getElementById("position");

jogNegYEle = document.getElementById("jog-y");
jogPosYEle = document.getElementById("jog+y");
jogNegXEle = document.getElementById("jog-x");
jogPosXEle = document.getElementById("jog+x");
penUpEle = document.getElementById("pen-up");
penDownEle = document.getElementById("pen-down");
jogAmountEle = document.getElementById("jog-amount");
resetXEle = document.getElementById("reset-x");
resetYEle = document.getElementById("reset-y");
resetEle = document.getElementById("reset");

pauseEle = document.getElementById("pause");
resumeEle = document.getElementById("resume");
stopEle = document.getElementById("stop");
uploadEle = document.getElementById("upload");
startEle = document.getElementById("start");
fileEle = document.getElementById("file");
svgContEle = document.getElementById("svg");
canvasContEle = document.getElementById("canvas");
canvasEle = canvasContEle.querySelector("canvas");

stepsEle = document.getElementById("steps");

const host = ["localhost:3000", "127.0.0.1:3000"].includes(window.location.host)
  ? "192.168.4.1"
  : window.location.host;
// connect to the websocket at /ws
console.log("Connecting to ws://" + host + "/ws");
let ws = new WebSocket(`ws://${host}/ws`);

// automatically reconnect if the connection is closed
function onWsClose() {
  console.log("WS connection closed, reconnecting in 1s");
  setTimeout(function () {
    ws = new WebSocket(`ws://${host}/ws`);
    ws.onopen = onWsOpen;
    ws.onmessage = onWsMessage;
    ws.onclose = onWsClose;
  }, 1000);
}
ws.onclose = onWsClose;

let status = "Idle";
let remoteNonce = -1;
let targetX = -1;
let targetY = -1;
let currentX = -1;
let currentY = -1;

let drawing = [];
let steps = [];
let startTime = 0;

let canvasW = 5550;
let canvasH = 5550;

let scaledCanvasW = 0;
let scaledCanvasH = 0;

let canvasContW = 0;
let canvasContH = 0;
let scaleFactor = 0;

function calculateCanvasScaleFactor() {
  canvasContH = canvasContEle.clientHeight;
  canvasContW = canvasContEle.clientWidth;

  // compute the scale factor with canvasContW/H and canvasW/H. maintain the aspect ratio
  const scaleW = canvasContW / canvasW;
  const scaleH = canvasContH / canvasH;
  scaleFactor = Math.min(scaleW, scaleH);

  scaledCanvasW = canvasW * scaleFactor;
  scaledCanvasH = canvasH * scaleFactor;
}

calculateCanvasScaleFactor();
canvasEle.width = scaledCanvasW;
canvasEle.height = scaledCanvasH;
drawToCanvas();

function updateStatus(s) {
  status = s;
  statusEle.innerText = status;

  if (status === "Idle") {
    progressEle.innerText = "-";
    elapsedEle.innerText = "-";

    uploadEle.classList.remove("hidden");
    startEle.classList.remove("hidden");
    stopEle.classList.add("hidden");
  } else if (status == "Drawing") {
    uploadEle.classList.add("hidden");
    startEle.classList.add("hidden");
    stopEle.classList.remove("hidden");
  }
}

function updatePosition(x, y) {
  positionEle.innerText = `(${x}, ${y})`;
  currentX = x;
  currentY = y;
}

function onWsOpen() {
  wsSend("ip");
  wsSend("nonce");
}

function wsSend(data) {
  console.log("Sending to WS:", data);
  ws.send(data);
}

function onWsMessage(event) {
  let data = event.data;
  console.log("Data from WS:", data);

  if (data.startsWith("IP: ")) {
    ipEle.innerText = data.split(": ")[1];
  } else if (data.startsWith("NONCE: ")) {
    remoteNonce = Number(data.split(": ")[1]);
  }
}
ws.onmessage = onWsMessage;

// fetch values on load
ws.onopen = onWsOpen;

function toBin(dec) {
  return dec.toString(2);
}

function setTargetX(x) {
  targetX = x;
  const bin = "00" + toBin(x).padStart(14, "0");
  return "d" + bin;
}

function setTargetY(y) {
  targetY = y;
  const bin = "01" + toBin(y).padStart(14, "0");
  return "d" + bin;
}

function goToTarget() {
  return "d1000000000000000";
}

function resetPos() {
  return "d1010000000000000";
}

function penUp() {
  return "d1111100000000000";
}

function penDown() {
  return "d1100000000000000";
}

async function runUntilComplete(command, posUpdate = false) {
  let data = "";

  if (command.type == "targetX") {
    data = setTargetX(command.value);
  } else if (command.type == "targetY") {
    data = setTargetY(command.value);
  } else if (command.type == "goToTarget") {
    data = goToTarget();
  } else if (command.type == "resetPos") {
    data = resetPos();
  } else if (command.type == "penUp") {
    data = penUp();
  } else if (command.type == "penDown") {
    data = penDown();
  } else if (command.type == "delay") {
    await delay(command.value);
    return;
  }

  drawToCanvas();
  oldNonce = remoteNonce;
  wsSend(data);
  while (remoteNonce === oldNonce) {
    await delay(10);
  }
  if (posUpdate) {
    updatePosition(targetX, targetY);
  }
  return;
}

function start() {
  console.log("Starting");

  updateStatus("Drawing");

  if (steps.length == 0) {
    console.log("No drawing to do");
    updateStatus("Idle");
    return;
  }

  startTime = Date.now();
  (async () => {
    for (let i = 0; i < steps.length; i++) {
      if (status == "Idle") {
        break;
      }
      await runUntilComplete(steps[i][0], steps[i][1]);
    }
    drawToCanvas();
    updateStatus("Idle");
  })();
}

startEle.onclick = start;
stopEle.onclick = () => {
  updateStatus("Idle");
};

async function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

resetEle.onclick = () => {
  let xPos = Number(resetXEle.value);
  let yPos = Number(resetYEle.value);
  if (isNaN(xPos) || isNaN(yPos)) {
    console.log("Invalid values");
    return;
  }
  console.log("Resetting to", xPos, yPos);
  (async () => {
    await runUntilComplete({ type: "targetX", value: xPos });
    await runUntilComplete({ type: "targetY", value: yPos });
    await runUntilComplete({ type: "resetPos" }, true);
    drawToCanvas();
  })();
};

jogNegXEle.onclick = () => {
  jog("-x");
};
jogPosXEle.onclick = () => {
  jog("+x");
};
jogNegYEle.onclick = () => {
  jog("-y");
};
jogPosYEle.onclick = () => {
  jog("+y");
};
penDownEle.onclick = () => {
  (async () => {
    await runUntilComplete({ type: "penDown" });
    drawToCanvas();
  })();
};
penUpEle.onclick = () => {
  (async () => {
    await runUntilComplete({ type: "penUp" });
    drawToCanvas();
  })();
};

function jog(dir) {
  let amount = Number(jogAmountEle.value);
  if (isNaN(amount)) {
    console.log("Invalid value");
    return;
  }

  if (currentX < 0 || currentY < 0) {
    console.log("Current position unknown");
    return;
  }

  let newX = currentX;
  let newY = currentY;
  if (dir == "-x") {
    newX -= amount;
  } else if (dir == "+x") {
    newX += amount;
  } else if (dir == "-y") {
    newY -= amount;
  } else if (dir == "+y") {
    newY += amount;
  }

  console.log("Jogging to", newX, newY);
  updateStatus("Drawing");
  (async () => {
    await runUntilComplete({ type: "targetX", value: newX });
    await runUntilComplete({ type: "targetY", value: newY });
    await runUntilComplete({ type: "goToTarget" }, true);
    drawToCanvas();
  })().then(() => {
    console.log("Done");
    updateStatus("Idle");
  });
}

uploadEle.onclick = () => {
  // prompt the user to select a file
  fileEle.click();
};

fileEle.onchange = (e) => {
  if (!e.target.files[0]) {
    console.log("No file selected");
    return;
  }
  const file = e.target.files[0];
  console.log("Selected file:", file);
  // read contents and add it to svgEle
  const reader = new FileReader();
  reader.onload = (e) => {
    const text = e.target.result;
    svgContEle.innerHTML = text;

    // get the svg element
    const svgEle = svgContEle.querySelector("svg");
    if (!svgEle) {
      console.log("Invalid SVG");
      return;
    }
    if (svgEle.querySelector("rect"))
      svgEle.removeChild(svgEle.querySelector("rect"));
    drawing = flattenSVG(svgEle, { maxError: 0.1 });
    console.log(drawing);
    parseDrawing();
    drawToCanvas();
  };
  reader.readAsText(file);
};

window.onresize = () => {
  calculateCanvasScaleFactor();
  canvasEle.width = scaledCanvasW;
  canvasEle.height = scaledCanvasH;
  drawToCanvas();
};

function parseDrawing() {
  steps = [];
  steps.push([{ type: "penDown" }, false]);
  steps.push([{ type: "delay", value: 1000 }, false]);
  drawing.forEach((line) => {
    steps.push([
      { type: "targetX", value: Math.round(line.points[0][0]) },
      false,
    ]);
    steps.push([
      { type: "targetY", value: Math.round(line.points[0][1]) },
      false,
    ]);
    steps.push([{ type: "goToTarget" }, true]);
    for (let i = 1; i < line.points.length; i++) {
      steps.push([
        { type: "targetX", value: Math.round(line.points[i][0]) },
        false,
      ]);
      steps.push([
        { type: "targetY", value: Math.round(line.points[i][1]) },
        false,
      ]);
      steps.push([{ type: "goToTarget" }, true]);
    }
  });
  steps.push([{ type: "penUp" }, false]);
}

function drawToCanvas() {
  const ctx = canvasEle.getContext("2d");
  ctx.clearRect(0, 0, scaledCanvasW, scaledCanvasH);

  ctx.strokeStyle = "black";
  ctx.lineWidth = 1;
  drawing.forEach((line) => {
    if (line.points.length < 1) return;
    let points = line.points;
    ctx.beginPath();
    ctx.moveTo(
      Math.round(points[0].x) * scaleFactor,
      Math.round(points[0].y) * scaleFactor
    );
    for (let i = 1; i < points.length; i++) {
      ctx.lineTo(
        Math.round(points[i].x) * scaleFactor,
        Math.round(points[i].y) * scaleFactor
      );
    }
    ctx.stroke();
  });
  // draw border in blue
  ctx.strokeStyle = "blue";
  ctx.lineWidth = 5;
  ctx.strokeRect(0, 0, scaledCanvasW, scaledCanvasH);

  // draw current position if it is known
  if (currentX >= 0 && currentY >= 0) {
    ctx.fillStyle = "red";
    ctx.beginPath();
    ctx.arc(currentX * scaleFactor, currentY * scaleFactor, 5, 0, 2 * Math.PI);
    ctx.fill();
    // if current position is not the target, draw a line to the target
    if (targetX >= 0 && targetY >= 0) {
      ctx.strokeStyle = "green";
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(currentX * scaleFactor, currentY * scaleFactor);
      ctx.lineTo(targetX * scaleFactor, targetY * scaleFactor);
      ctx.stroke();
    }
  }
}
