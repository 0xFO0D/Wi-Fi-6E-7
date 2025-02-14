document.addEventListener('DOMContentLoaded', () => {
    // Navigation
    const navItems = document.querySelectorAll('.nav-item');
    const sections = document.querySelectorAll('section');

    navItems.forEach(item => {
        item.addEventListener('click', () => {
            const targetSection = item.dataset.section;
            
            // Update active states
            navItems.forEach(nav => nav.classList.remove('active'));
            sections.forEach(section => section.classList.remove('active'));
            
            item.classList.add('active');
            document.getElementById(targetSection).classList.add('active');
            
            // Add terminal typing effect
            if (targetSection === 'about') {
                typeTerminalText();
            }
        });
    });

    // Terminal typing effect
    function typeTerminalText() {
        const aboutText = document.querySelector('.about-text');
        const text = aboutText.textContent;
        aboutText.textContent = '';
        let i = 0;

        function type() {
            if (i < text.length) {
                aboutText.textContent += text.charAt(i);
                i++;
                setTimeout(type, 30);
            }
        }

        type();
    }

    // Documentation section interaction
    const docItems = document.querySelectorAll('.doc-item');
    const docContent = document.querySelector('.doc-content pre');
    
    const docTexts = {
        'Getting Started': `
> INSTALLATION
1. Clone the repository
2. Run make
3. Load the driver module

> REQUIREMENTS
- Linux kernel 5.15+
- Build tools
- Wireless tools`,
        'Driver Architecture': `
> ARCHITECTURE OVERVIEW
+------------------+
|    Core Layer    |
+------------------+
|    MAC Layer     |
+------------------+
|    PHY Layer     |
+------------------+
|  Hardware Layer  |
+------------------+`,
        'MLO Implementation': `
> MLO FEATURES
- Multi-link operation
- Dynamic link selection
- Power state management
- Traffic distribution
- Link aggregation`,
        'Power Management': `
> POWER FEATURES
- EMLSR support
- EMLPS support
- Dynamic power states
- QoS integration
- Latency management`,
        'Hardware Support': `
> SUPPORTED HARDWARE
- Wi-Fi 6E chipsets
- Wi-Fi 7 chipsets
- Multiple vendors
- USB/PCI support`
    };

    docItems.forEach(item => {
        item.addEventListener('click', () => {
            const text = docTexts[item.textContent];
            typeDocContent(text);
            
            docItems.forEach(di => di.classList.remove('active'));
            item.classList.add('active');
        });
    });

    function typeDocContent(text) {
        docContent.textContent = '';
        let i = 0;

        function type() {
            if (i < text.length) {
                docContent.textContent += text.charAt(i);
                i++;
                setTimeout(type, 20);
            }
        }

        type();
    }

    // Add CRT flicker effect
    const overlay = document.querySelector('.crt-overlay');
    
    function updateFlicker() {
        const flickerAmount = 0.97 + Math.random() * 0.03;
        overlay.style.opacity = flickerAmount;
        requestAnimationFrame(updateFlicker);
    }

    updateFlicker();

    // Initialize first section
    typeTerminalText();
}); 