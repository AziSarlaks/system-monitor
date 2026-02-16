class SystemCharts {
    constructor() {
        this.charts = {};
        this.init();
    }

    init() {
        // Инициализация дополнительных графиков
        this.setupTooltips();
    }

    setupTooltips() {
        // Добавляем всплывающие подсказки для элементов
        document.addEventListener('mouseover', (e) => {
            if (e.target.classList.contains('has-tooltip')) {
                this.showTooltip(e.target, e.target.dataset.tooltip);
            }
        });

        document.addEventListener('mouseout', (e) => {
            if (e.target.classList.contains('has-tooltip')) {
                this.hideTooltip();
            }
        });
    }

    showTooltip(element, text) {
        let tooltip = document.getElementById('sysmon-tooltip');
        if (!tooltip) {
            tooltip = document.createElement('div');
            tooltip.id = 'sysmon-tooltip';
            tooltip.className = 'sysmon-tooltip';
            document.body.appendChild(tooltip);
        }
        
        tooltip.textContent = text;
        tooltip.style.display = 'block';
        
        const rect = element.getBoundingClientRect();
        tooltip.style.left = `${rect.left + window.scrollX}px`;
        tooltip.style.top = `${rect.top + window.scrollY - 30}px`;
    }

    hideTooltip() {
        const tooltip = document.getElementById('sysmon-tooltip');
        if (tooltip) {
            tooltip.style.display = 'none';
        }
    }

    createDiskChart(containerId, diskData) {
        const ctx = document.getElementById(containerId)?.getContext('2d');
        if (!ctx) return null;
        
        this.charts.disk = new Chart(ctx, {
            type: 'bar',
            data: {
                labels: diskData.map(d => d.device),
                datasets: [
                    {
                        label: 'Read MB/s',
                        data: diskData.map(d => d.read_mb),
                        backgroundColor: '#4CAF50',
                        borderColor: '#388E3C',
                        borderWidth: 1,
                        borderRadius: 5
                    },
                    {
                        label: 'Write MB/s',
                        data: diskData.map(d => d.write_mb),
                        backgroundColor: '#2196F3',
                        borderColor: '#1976D2',
                        borderWidth: 1,
                        borderRadius: 5
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        position: 'top',
                        labels: {
                            usePointStyle: true,
                            padding: 20
                        }
                    },
                    tooltip: {
                        callbacks: {
                            label: function(context) {
                                return `${context.dataset.label}: ${context.parsed.y.toFixed(2)} MB/s`;
                            }
                        }
                    }
                },
                scales: {
                    y: {
                        beginAtZero: true,
                        grid: {
                            color: 'rgba(0, 0, 0, 0.05)'
                        },
                        title: {
                            display: true,
                            text: 'MB/s',
                            color: '#666'
                        },
                        ticks: {
                            color: '#666'
                        }
                    },
                    x: {
                        grid: {
                            color: 'rgba(0, 0, 0, 0.05)'
                        },
                        ticks: {
                            color: '#666'
                        }
                    }
                }
            }
        });
        
        return this.charts.disk;
    }

    createNetworkChart(containerId, networkData) {
        const ctx = document.getElementById(containerId)?.getContext('2d');
        if (!ctx) return null;
        
        this.charts.network = new Chart(ctx, {
            type: 'line',
            data: {
                labels: networkData.map(d => d.time),
                datasets: [
                    {
                        label: 'Download KB/s',
                        data: networkData.map(d => d.rx_kb),
                        borderColor: '#4CAF50',
                        backgroundColor: 'rgba(76, 175, 80, 0.1)',
                        borderWidth: 2,
                        fill: true,
                        tension: 0.4,
                        pointRadius: 0
                    },
                    {
                        label: 'Upload KB/s',
                        data: networkData.map(d => d.tx_kb),
                        borderColor: '#2196F3',
                        backgroundColor: 'rgba(33, 150, 243, 0.1)',
                        borderWidth: 2,
                        fill: true,
                        tension: 0.4,
                        pointRadius: 0
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        position: 'top',
                        labels: {
                            usePointStyle: true,
                            padding: 20
                        }
                    },
                    tooltip: {
                        mode: 'index',
                        intersect: false,
                        callbacks: {
                            label: function(context) {
                                return `${context.dataset.label}: ${context.parsed.y.toFixed(2)} KB/s`;
                            }
                        }
                    }
                },
                scales: {
                    y: {
                        beginAtZero: true,
                        grid: {
                            color: 'rgba(0, 0, 0, 0.05)'
                        },
                        title: {
                            display: true,
                            text: 'KB/s',
                            color: '#666'
                        },
                        ticks: {
                            color: '#666'
                        }
                    },
                    x: {
                        grid: {
                            color: 'rgba(0, 0, 0, 0.05)'
                        },
                        ticks: {
                            color: '#666',
                            maxRotation: 0
                        }
                    }
                },
                animation: {
                    duration: 0
                }
            }
        });
        
        return this.charts.network;
    }

    createProcessTreeChart(containerId, processes) {
        const ctx = document.getElementById(containerId)?.getContext('2d');
        if (!ctx) return null;
        
        // Группируем процессы по имени для трешчарта
        const grouped = {};
        processes.forEach(proc => {
            const name = proc.name || 'unknown';
            grouped[name] = (grouped[name] || 0) + (proc.memory || 0);
        });
        
        const labels = Object.keys(grouped);
        const data = Object.values(grouped);
        
        // Цветовая схема
        const colors = this.generateColors(labels.length);
        
        this.charts.processTree = new Chart(ctx, {
            type: 'doughnut',
            data: {
                labels: labels,
                datasets: [{
                    data: data,
                    backgroundColor: colors,
                    borderWidth: 1,
                    borderColor: '#fff',
                    hoverOffset: 15
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        position: 'right',
                        labels: {
                            boxWidth: 12,
                            padding: 15,
                            usePointStyle: true
                        }
                    },
                    tooltip: {
                        callbacks: {
                            label: function(context) {
                                const value = context.parsed;
                                const total = context.dataset.data.reduce((a, b) => a + b, 0);
                                const percentage = ((value / total) * 100).toFixed(1);
                                return `${context.label}: ${formatBytes(value)} (${percentage}%)`;
                            }
                        }
                    }
                },
                cutout: '50%'
            }
        });
        
        return this.charts.processTree;
    }

    generateColors(count) {
        const colors = [];
        const hueStep = 360 / count;
        
        for (let i = 0; i < count; i++) {
            const hue = (i * hueStep) % 360;
            colors.push(`hsl(${hue}, 70%, 60%)`);
        }
        
        return colors;
    }

    updateDiskChart(diskData) {
        if (this.charts.disk) {
            this.charts.disk.data.labels = diskData.map(d => d.device);
            this.charts.disk.data.datasets[0].data = diskData.map(d => d.read_mb);
            this.charts.disk.data.datasets[1].data = diskData.map(d => d.write_mb);
            this.charts.disk.update();
        }
    }

    updateNetworkChart(networkData) {
        if (this.charts.network) {
            this.charts.network.data.labels = networkData.map(d => d.time);
            this.charts.network.data.datasets[0].data = networkData.map(d => d.rx_kb);
            this.charts.network.data.datasets[1].data = networkData.map(d => d.tx_kb);
            this.charts.network.update();
        }
    }

    updateProcessTreeChart(processes) {
        if (this.charts.processTree) {
            const grouped = {};
            processes.forEach(proc => {
                const name = proc.name || 'unknown';
                grouped[name] = (grouped[name] || 0) + (proc.memory || 0);
            });
            
            const labels = Object.keys(grouped);
            const data = Object.values(grouped);
            const colors = this.generateColors(labels.length);
            
            this.charts.processTree.data.labels = labels;
            this.charts.processTree.data.datasets[0].data = data;
            this.charts.processTree.data.datasets[0].backgroundColor = colors;
            this.charts.processTree.update();
        }
    }

    destroyChart(chartName) {
        if (this.charts[chartName]) {
            this.charts[chartName].destroy();
            delete this.charts[chartName];
        }
    }

    destroyAllCharts() {
        Object.keys(this.charts).forEach(chartName => {
            this.destroyChart(chartName);
        });
        this.charts = {};
    }
}

// Вспомогательные функции
function formatBytes(bytes, decimals = 2) {
    if (bytes === 0) return '0 Bytes';
    
    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
    
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

function formatBytesPerSecond(bytes, decimals = 2) {
    if (bytes === 0) return '0 B/s';
    
    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['B/s', 'KB/s', 'MB/s', 'GB/s', 'TB/s'];
    
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

function getColorForValue(value, max = 100) {
    const percentage = value / max;
    
    if (percentage > 0.8) return '#f44336'; // Красный
    if (percentage > 0.6) return '#ff9800'; // Оранжевый
    if (percentage > 0.4) return '#ffc107'; // Желтый
    if (percentage > 0.2) return '#8bc34a'; // Светло-зеленый
    return '#4CAF50'; // Зеленый
}

function animateValue(element, start, end, duration = 500, suffix = '') {
    if (start === end) return;
    
    const startTime = performance.now();
    
    function update(currentTime) {
        const elapsed = currentTime - startTime;
        const progress = Math.min(elapsed / duration, 1);
        
        // Easing function
        const easeOutQuart = 1 - Math.pow(1 - progress, 4);
        const currentValue = start + (end - start) * easeOutQuart;
        
        element.textContent = currentValue.toFixed(1) + suffix;
        
        if (progress < 1) {
            requestAnimationFrame(update);
        }
    }
    
    requestAnimationFrame(update);
}

function createProgressBar(value, max = 100, width = 100) {
    const percentage = (value / max) * 100;
    const color = getColorForValue(value, max);
    
    const container = document.createElement('div');
    container.style.width = `${width}px`;
    container.style.height = '6px';
    container.style.backgroundColor = '#f0f0f0';
    container.style.borderRadius = '3px';
    container.style.overflow = 'hidden';
    container.style.position = 'relative';
    
    const bar = document.createElement('div');
    bar.style.width = `${percentage}%`;
    bar.style.height = '100%';
    bar.style.backgroundColor = color;
    bar.style.borderRadius = '3px';
    bar.style.transition = 'width 0.3s ease';
    
    container.appendChild(bar);
    
    return container;
}

// Глобальная инициализация
document.addEventListener('DOMContentLoaded', () => {
    window.sysmonCharts = new SystemCharts();
    
    // Добавляем CSS для тултипов
    const style = document.createElement('style');
    style.textContent = `
        .sysmon-tooltip {
            position: absolute;
            background: rgba(0, 0, 0, 0.8);
            color: white;
            padding: 5px 10px;
            border-radius: 4px;
            font-size: 12px;
            pointer-events: none;
            z-index: 1000;
            display: none;
            white-space: nowrap;
            max-width: 300px;
            overflow: hidden;
            text-overflow: ellipsis;
        }
        
        .sysmon-tooltip::after {
            content: '';
            position: absolute;
            top: 100%;
            left: 50%;
            margin-left: -5px;
            border-width: 5px;
            border-style: solid;
            border-color: rgba(0, 0, 0, 0.8) transparent transparent transparent;
        }
    `;
    document.head.appendChild(style);
});

// Экспорт для глобального использования
window.SystemCharts = SystemCharts;
window.SysmonUtils = {
    formatBytes,
    formatBytesPerSecond,
    getColorForValue,
    animateValue,
    createProgressBar
};