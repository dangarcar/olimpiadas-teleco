const center = [20, 55];

const map = new maplibregl.Map({
    container: 'map',
    style: 'https://demotiles.maplibre.org/style.json', // stylesheet location
    center: center, // starting position [lng, lat]
    zoom: 2.5, // starting zoom
    antialias: true,
});

let xValues = [];
let yValues = [];

generateData("Math.sin(x)", 0, 10, 0.5);
const chart1 = new Chart("chart1", {
    type: "line",
    data: {
        labels: xValues,
        datasets: [{
            fill: false,
            pointRadius: 1,
            borderColor: "rgba(255,0,0,0.5)",
            data: yValues
        }]
    },
    options: {
        legend: {display: false},
        title: {
            display: true,
            text: 'Sine',
        }
    }
});

xValues = [];
yValues = [];
generateData("Math.cos(x)", 0, 10, 0.5);
const chart2 = new Chart("chart2", {
    type: "line",
    data: {
        labels: xValues,
        datasets: [{
            fill: false,
            pointRadius: 1,
            borderColor: "rgba(255,0,0,0.5)",
            data: yValues
        }]
    },
    options: {
        legend: {display: false},
        title: {
            display: true,
            text: 'Cosine'
        }
    }
});

xValues = [];
yValues = [];
generateData("Math.pow(x, 2)", -5, 5, 0.5);
const chart3 = new Chart("chart3", {
    type: "line",
    data: {
        labels: xValues,
        datasets: [{
            fill: false,
            pointRadius: 1,
            borderColor: "rgba(255,0,0,0.5)",
            data: yValues
        }]
    },
    options: {
        legend: {display: false},
        title: {
            display: true,
            text: 'Quadratic'
        }
    }
});

function generateData(value, i1, i2, step = 1) {
    for (let x = i1; x <= i2; x += step) {
        yValues.push(eval(value));
        xValues.push(x);
    }
}