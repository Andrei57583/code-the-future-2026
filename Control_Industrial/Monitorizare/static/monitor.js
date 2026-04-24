const ctx = document.getElementById('chart_monitor').getContext('2d');
const sensorSelect = document.getElementById('sensor-select');
const maxDataPoints = 20;

const myChart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [{
            label: 'Temperatura (C)',
            data: [], // Aici vor fi valorile(msg.data)
            borderColor: '#3498db',
            backgroundColor: 'rgba(166, 241, 183, 0.84)',
            fill: true,
            tension: 0.4 // Face linia curbată (smooth)
        }]
    },
    options: {
        responsive: true,
        mantainAspectRatio: false,
        scales: {
            y:{
                beginAtZero: false,
                grid: {
                    color: '#ecf0f1'
                },
                ticks: {
                    color: '#ecf0f1'
                }
            },
            x:{
                beginAtZero: false,
                grid: {
                    color: '#ecf0f1'
                },
                ticks: {
                    color: '#ecf0f1'
                }
            }
        },
        plugins: {
            legend: {
                labels: {
                    boxWidth: 10,
                    font: {size: 11},
                    color: '#ecf0f1'
                }
                
            }
        }
    }
});

sensorSelect.addEventListener('change', () => {
    myChart.data.labels=[];
    myChart.data.datasets[0].data = [];

    const selectedText = sensorSelect.options[sensorSelect.selectedIndex].text;
    myChart.data.datasets[0].label =selectedText;

    myChart.update();
})

const socket = io();

socket.onAny((eventName, ...args)=> {
    console.log(`Eveniment primit: ${eventName}`, args);
})

socket.on('connect', () => {
    console.log("Conectat la serverul local.")
})

socket.on('mqtt_update', function(msg) {

    const nivel_risc = document.getElementById('risk');

    const currentSensor = sensorSelect.value;
    
    // 1. Se extrag valorile {"data":valoare} din python
    let date = msg.data ? msg.data : msg;

    let risc = date.r;

    console.log("Verific risc: ", risc);

    if (risc === "SAFE") {
        nivel_risc.style.backgroundColor = "#02f212";
    } else if (risc === "MODERATE") {
        nivel_risc.style.backgroundColor = "#eef202";
    } else if (risc === "CRITICAL") {
        nivel_risc.style.backgroundColor = "#f20202";
    } 


    let valoare = msg.data[currentSensor];

    if (valoare != null && valoare != undefined && typeof valoare == 'number') {
        let timp = new Date().toLocaleTimeString();

        // 2. Se adauga datele noi in grafic
        myChart.data.labels.push(timp);
        myChart.data.datasets[0].data.push(valoare);

        // 3. Scroll effect 
        if (myChart.data.labels.length > maxDataPoints) {
            myChart.data.labels.shift();
            myChart.data.datasets[0].data.shift();
        }

        // 4. Se actualizeaza graficul
        myChart.update();

        console.log(`Plotting ${currentSensor}: ${valoare}`);
    } else {
        console.log("Valoarea nu este valida pentru grafic: ", valoare);
    } 

});

socket.on('connection_error', (err) => {
    console.log("Eroare conexiune:", err);
})
